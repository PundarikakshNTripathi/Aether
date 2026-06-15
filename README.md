# Aether

A distributed, multimodal web search engine built for the AI era. 

Aether replaces traditional inverted-index search by combining a high-concurrency Go web crawler, a C++ HNSW vector retrieval database, and a dedicated C++ Vision-Language Model (VLM) engine for deep semantic contextualization.

## Architecture
* **Frontend:** Next.js / React
* **Crawler Node:** Go + Redis
* **Search Core:** C++20 (BM25 + HNSW Graph)
* **VLM Engine:** C++ (Dedicated Inference Service)
* **Communication:** gRPC / Protocol Buffers

## Orchestration & Deployment

Aether supports two methods for local development and orchestration:

### Method 1: Docker Compose / Podman Compose (Recommended)
This approach leverages standard Compose files to spin up the cluster with network isolation.

```bash
# 1. Generate the gRPC stubs
make proto

# 2. Build and start the services in the background
make up

# 3. Stop the services
make down
```

### Method 2: Native Podman Pod Orchestration
Useful on hosts lacking Compose plugins or when testing pod-level network sharing (all services share `localhost` inside the pod).

```bash
# 1. Build and run the services inside a Podman pod
make run-podman-pod

# 2. Stop and remove the pod and its containers
make stop-podman-pod
```

---

## Troubleshooting & System Configuration

### 1. Podman API Socket (for Compose Compatibility)
If you encounter errors connecting to the Docker API socket while running `make up`, verify that your user-level Podman socket daemon is active:
```bash
systemctl --user enable --now podman.socket
```

### 2. "Container Name Already in Use"
If a previous execution terminated abruptly or left dangling containers, running `make run-podman-pod` might encounter namespace conflicts. The Makefile is configured to use the `--replace` flag to automatically clear and replace these containers. If you need to manually purge them, run:
```bash
make stop-podman-pod
```

### 3. Rootless Podman on Btrfs Filesystems
If building under rootless Podman on a Btrfs filesystem (e.g., Fedora defaults) fails with `Digest did not match` during layer assembly, apply the following system configuration:

1. **Disable Copy-on-Write (CoW)** on Podman's local store folder before creating images (CoW metadata and overlayfs layer diffing can conflict on Btrfs):
   ```bash
   mkdir -p ~/.local/share/containers/storage
   chattr +C ~/.local/share/containers/storage
   ```
2. **Move the temporary image staging directory** to `tmpfs` (RAM disk) to bypass physical disk locking and checksum issues. Add the following to your user-level `~/.config/containers/containers.conf`:
   ```toml
   [engine]
   env = ["TMPDIR=/tmp"]
   ```
3. Restart the user socket to load the changes:
   ```bash
   systemctl --user restart podman.socket
   ```
