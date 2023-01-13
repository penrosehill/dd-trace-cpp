from golang:1.20rc1-alpine3.17 as build

workdir /usr/local/src/agent
copy agent.go /usr/local/src/agent/main.go
run go build -v -o /usr/local/bin/agent /usr/local/src/agent/main.go

from alpine:3.17
copy --from=build /usr/local/bin/agent /usr/local/bin/agent
cmd ["/usr/local/bin/agent"]
