from golang:1.20rc1-alpine3.17 as build

workdir /usr/local/src/upstream
copy upstream.go /usr/local/src/upstream/main.go
run go build -v -o /usr/local/bin/upstream /usr/local/src/upstream/main.go

from alpine:3.17
copy --from=build /usr/local/bin/upstream /usr/local/bin/upstream
cmd ["/usr/local/bin/upstream"]
