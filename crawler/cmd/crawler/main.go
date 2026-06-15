package main

import (
	"context"
	"log"
	"net/http"
	"os"
	"os/signal"
	"strconv"
	"sync"
	"syscall"
	"time"

	"github.com/redis/go-redis/v9"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"

	"crawler/internal/parser"
	pb "crawler/internal/pb"
	"crawler/internal/queue"
)

type Crawler struct {
	redisClient *redis.Client
	queue       *queue.FrontierQueue
	searchCore  pb.CrawlerServiceClient
	httpClient  *http.Client
	concurrency int
	seedURLs    []string
}

func main() {
	redisAddr := getEnv("REDIS_ADDR", "localhost:6379")
	searchCoreAddr := getEnv("SEARCH_CORE_ADDR", "localhost:50051")
	concurrencyStr := getEnv("CONCURRENCY", "5")
	seedURL := getEnv("SEED_URL", "https://en.wikipedia.org/wiki/Information_retrieval")

	concurrency, err := strconv.Atoi(concurrencyStr)
	if err != nil {
		concurrency = 5
	}

	log.Printf("Starting Aether Go Crawler...")
	log.Printf("Redis Address: %s", redisAddr)
	log.Printf("Search Core Address: %s", searchCoreAddr)
	log.Printf("Concurrency Level: %d", concurrency)
	log.Printf("Seed URL: %s", seedURL)

	// 1. Initialize Redis Client
	rdb := redis.NewClient(&redis.Options{
		Addr: redisAddr,
	})
	
	// Test Redis connection
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	if err := rdb.Ping(ctx).Err(); err != nil {
		log.Printf("Warning: Failed to connect to Redis at startup: %v. Will retry during execution.", err)
	}

	// 2. Connect to Search Core via gRPC
	conn, err := grpc.Dial(searchCoreAddr, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		log.Fatalf("Failed to dial Search Core: %v", err)
	}
	defer conn.Close()
	searchClient := pb.NewCrawlerServiceClient(conn)

	// 3. Setup Crawler
	crawler := &Crawler{
		redisClient: rdb,
		queue:       queue.NewFrontierQueue(rdb, "aether_crawler_queue", "aether_crawler_visited"),
		searchCore:  searchClient,
		httpClient: &http.Client{
			Timeout: 10 * time.Second,
		},
		concurrency: concurrency,
		seedURLs:    []string{seedURL},
	}

	// Clear queue and inject seed URL for fresh start
	if err := crawler.queue.Reset(context.Background()); err != nil {
		log.Printf("Error resetting queue: %v", err)
	}
	if _, err := crawler.queue.Push(context.Background(), seedURL); err != nil {
		log.Printf("Error adding seed URL: %v", err)
	}

	// 4. Start Crawling with context cancellation support
	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()

	var wg sync.WaitGroup
	for i := 0; i < crawler.concurrency; i++ {
		wg.Add(1)
		go func(workerID int) {
			defer wg.Done()
			crawler.worker(ctx, workerID)
		}(i)
	}

	// Wait for shutdown or workers completion
	wg.Wait()
	log.Println("Crawler shut down gracefully.")
}

func (c *Crawler) worker(ctx context.Context, id int) {
	log.Printf("[Worker %d] Started", id)
	for {
		select {
		case <-ctx.Done():
			log.Printf("[Worker %d] Stopping due to context cancellation", id)
			return
		default:
			// Pop next URL from Redis queue (blocks for up to 2 seconds)
			url, err := c.queue.Pop(ctx, 2*time.Second)
			if err != nil {
				log.Printf("[Worker %d] Queue pop error: %v", id, err)
				time.Sleep(1 * time.Second)
				continue
			}

			if url == "" {
				// Queue is currently empty, sleep and retry
				time.Sleep(500 * time.Millisecond)
				continue
			}

			log.Printf("[Worker %d] Crawling: %s", id, url)
			c.processURL(ctx, id, url)
		}
	}
}

func (c *Crawler) processURL(ctx context.Context, workerID int, url string) {
	req, err := http.NewRequestWithContext(ctx, "GET", url, nil)
	if err != nil {
		log.Printf("[Worker %d] Request creation error for %s: %v", workerID, url, err)
		return
	}
	// Identify crawler politely
	req.Header.Set("User-Agent", "AetherCrawler/1.0 (+http://localhost:3000)")

	resp, err := c.httpClient.Do(req)
	if err != nil {
		log.Printf("[Worker %d] HTTP fetch error for %s: %v", workerID, url, err)
		return
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		log.Printf("[Worker %d] Non-200 HTTP code %d for %s", workerID, resp.StatusCode, url)
		return
	}

	// Parse HTML content
	parsed, err := parser.ParseHTML(resp.Body, url)
	if err != nil {
		log.Printf("[Worker %d] HTML parsing error for %s: %v", workerID, url, err)
		return
	}

	log.Printf("[Worker %d] Extracted title: \"%s\", text length: %d, links: %d, images: %d", 
		workerID, parsed.Title, len(parsed.RawText), len(parsed.Links), len(parsed.Images))

	// Push extracted links to queue
	for _, link := range parsed.Links {
		pushed, err := c.queue.Push(ctx, link)
		if err != nil {
			log.Printf("[Worker %d] Redis push error for link %s: %v", workerID, link, err)
			continue
		}
		if pushed {
			log.Printf("[Worker %d] Discovered new link: %s", workerID, link)
		}
	}

	// Construct gRPC index page request
	images := make([]*pb.ImageInfo, 0, len(parsed.Images))
	for _, img := range parsed.Images {
		images = append(images, &pb.ImageInfo{
			Url:     img.URL,
			AltText: img.AltText,
		})
	}

	indexReq := &pb.IndexPageRequest{
		Url:     url,
		Title:   parsed.Title,
		RawText: parsed.RawText,
		Images:  images,
	}

	// Ingest into Search Core
	grpcCtx, cancel := context.WithTimeout(ctx, 5*time.Second)
	defer cancel()
	indexResp, err := c.searchCore.IndexPage(grpcCtx, indexReq)
	if err != nil {
		log.Printf("[Worker %d] Ingestion gRPC error for %s: %v", workerID, url, err)
		return
	}

	if !indexResp.Success {
		log.Printf("[Worker %d] Ingestion failed for %s: %s", workerID, url, indexResp.ErrorMessage)
	} else {
		log.Printf("[Worker %d] Successfully ingested and indexed: %s", workerID, url)
	}
}

func getEnv(key, defaultVal string) string {
	if value, exists := os.LookupEnv(key); exists {
		return value
	}
	return defaultVal
}
