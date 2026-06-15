.PHONY: proto build up down clean run-podman-pod stop-podman-pod

DOCKER := $(shell which podman 2>/dev/null || which docker 2>/dev/null)

# Fallback to standard docker/podman if compose isn't available
COMPOSE := $(shell which docker-compose 2>/dev/null || which podman-compose 2>/dev/null || echo "$(DOCKER) compose")

proto:
	@echo "Building proto generator container..."
	$(DOCKER) build -t aether-proto-builder -f Dockerfile.proto .
	@echo "Generating stubs for Go, C++, and TypeScript..."
	$(DOCKER) run --rm -v $$(pwd):/workspace:z aether-proto-builder
	@echo "Stubs successfully generated."

build: proto
	@echo "Building services..."
	$(COMPOSE) build

up: proto
	@echo "Starting the Aether cluster..."
	$(COMPOSE) up --build -d

down:
	@echo "Stopping the Aether cluster..."
	$(COMPOSE) down

# Podman-specific local pod orchestration (useful when compose plugins are missing)
run-podman-pod: proto
	@echo "Creating Podman pod (sharing localhost network)..."
	-$(DOCKER) pod create --name aether-pod -p 3000:3000 -p 50051:50051 -p 50052:50052
	
	@echo "Building images..."
	$(DOCKER) build -t aether-redis -f - . < <(echo "FROM docker.io/library/redis:alpine")
	$(DOCKER) build -t aether-search-core -f search_core/Dockerfile search_core
	$(DOCKER) build -t aether-vlm-engine -f vlm_engine/Dockerfile vlm_engine
	$(DOCKER) build -t aether-crawler -f crawler/Dockerfile crawler
	$(DOCKER) build -t aether-web -f web/Dockerfile web

	@echo "Running Redis..."
	$(DOCKER) run -d --replace --pod aether-pod --name aether-redis-node aether-redis
	
	@echo "Running Search Core..."
	$(DOCKER) run -d --replace --pod aether-pod --name aether-search-node aether-search-core
	
	@echo "Running VLM Engine..."
	$(DOCKER) run -d --replace --pod aether-pod --name aether-vlm-node aether-vlm-engine
	
	@echo "Running Web Frontend..."
	$(DOCKER) run -d --replace --pod aether-pod --name aether-web-node -e REDIS_ADDR=127.0.0.1:6379 -e SEARCH_CORE_ADDR=127.0.0.1:50051 -e VLM_ENGINE_ADDR=127.0.0.1:50052 aether-web
	
	@echo "Running Go Crawler..."
	$(DOCKER) run -d --replace --pod aether-pod --name aether-crawler-node -e REDIS_ADDR=127.0.0.1:6379 -e SEARCH_CORE_ADDR=127.0.0.1:50051 aether-crawler

stop-podman-pod:
	-$(DOCKER) rm -f aether-redis-node aether-search-node aether-vlm-node aether-web-node aether-crawler-node
	-$(DOCKER) pod rm aether-pod

clean:
	rm -rf crawler/internal/pb/*
	rm -rf search_core/src/pb/*
	rm -rf vlm_engine/src/pb/*
	rm -rf web/lib/pb/*
