FROM docker.io/library/golang:1.22-alpine AS builder

RUN apk add --no-cache git

WORKDIR /workspace

# Copy go.mod and dependencies first to leverage caching
COPY go.mod go.mod
COPY go.sum go.sum
RUN go mod download

# Copy source code and internal packages
COPY cmd cmd
COPY internal internal

# Compile binary
RUN CGO_ENABLED=0 GOOS=linux go build -o crawler ./cmd/crawler/main.go

FROM docker.io/library/alpine:latest

# Install basic certificates for HTTPS scraping
RUN apk --no-cache add ca-certificates

WORKDIR /app
COPY --from=builder /workspace/crawler .

CMD ["./crawler"]
