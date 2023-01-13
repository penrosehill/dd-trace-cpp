package main

import (
	"bufio"
	"fmt"
	"net/http"
	"os"
	"os/signal"
	"syscall"
)

var outputFile *os.File
var output *bufio.Writer

func headers(w http.ResponseWriter, req *http.Request) {
	var size int
	for name, headers := range req.Header {
		size += len(name)
		for _, h := range headers {
			size += len(h)
		}
	}
	fmt.Fprintln(output, size)
	_, err := w.Write([]byte{})
	if err != nil {
		fmt.Println("error:", err)
	}
}

func main() {
	path, ok := os.LookupEnv("BENCH_OUTPUT")
	if !ok {
		panic("Missing BENCH_OUTPUT environment variable")
	}
	outputFile, err := os.Create(path)
	if err != nil {
		panic(fmt.Sprintf("Unable to create/append %v because %v", path, err))
	}
	defer outputFile.Close()
	output = bufio.NewWriter(outputFile)

	signals := make(chan os.Signal, 1)
	signal.Notify(signals, syscall.SIGINT)
	go func() {
		<-signals
		output.Flush()
		outputFile.Close()
		os.Exit(0)
	}()

	http.HandleFunc("/", headers)
	http.ListenAndServe(":80", nil)
}
