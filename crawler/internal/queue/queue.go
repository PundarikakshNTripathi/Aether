package queue

import (
	"context"
	"errors"
	"time"

	"github.com/redis/go-redis/v9"
)

type FrontierQueue struct {
	client     *redis.Client
	queueKey   string
	visitedKey string
}

func NewFrontierQueue(client *redis.Client, queueKey, visitedKey string) *FrontierQueue {
	return &FrontierQueue{
		client:     client,
		queueKey:   queueKey,
		visitedKey: visitedKey,
	}
}

// Push adds a URL to the frontier if it hasn't been visited before
func (q *FrontierQueue) Push(ctx context.Context, url string) (bool, error) {
	// Add to visited set first (de-duplication)
	added, err := q.client.SAdd(ctx, q.visitedKey, url).Result()
	if err != nil {
		return false, err
	}

	if added == 0 {
		// Already visited
		return false, nil
	}

	// Push to FIFO queue list
	err = q.client.RPush(ctx, q.queueKey, url).Err()
	if err != nil {
		return false, err
	}

	return true, nil
}

// Pop retrieves the next URL to crawl, blocking for up to timeout if empty
func (q *FrontierQueue) Pop(ctx context.Context, timeout time.Duration) (string, error) {
	res, err := q.client.BLPop(ctx, timeout, q.queueKey).Result()
	if err != nil {
		if errors.Is(err, redis.Nil) {
			return "", nil // Timeout occurred, no element found
		}
		return "", err
	}

	if len(res) < 2 {
		return "", errors.New("invalid BLPop response format")
	}

	return res[1], nil
}

// Size returns the number of items remaining in the queue
func (q *FrontierQueue) Size(ctx context.Context) (int64, error) {
	return q.client.LLen(ctx, q.queueKey).Result()
}

// Reset clears the frontier queue and visited set
func (q *FrontierQueue) Reset(ctx context.Context) error {
	return q.client.Del(ctx, q.queueKey, q.visitedKey).Err()
}
