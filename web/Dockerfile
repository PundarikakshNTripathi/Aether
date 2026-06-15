FROM docker.io/library/node:20-alpine AS builder

WORKDIR /workspace

COPY package.json package.json
RUN npm install

COPY . .

# Run next build
RUN npm run build

FROM docker.io/library/node:20-alpine AS runner

WORKDIR /app

ENV NODE_ENV=production

COPY --from=builder /workspace/package.json ./
COPY --from=builder /workspace/node_modules ./node_modules
COPY --from=builder /workspace/.next ./.next
COPY --from=builder /workspace/public ./public
# Wait! Next.js build output includes a layout and components folder. We copy everything needed.
# Since we are copying .next and node_modules, copying package.json and running is standard.
# Let's ensure it can start next.
COPY --from=builder /workspace/app ./app

EXPOSE 3000

CMD ["npx", "next", "start"]
