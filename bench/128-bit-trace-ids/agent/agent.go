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
	contentLength := "0"

	lengths := req.Header["Content-Length"]
	if lengths != nil {
		if len(lengths) != 1 {
			panic(fmt.Sprintf("Content-Length has %d separate values: %v", len(lengths), lengths))
		}
		contentLength = lengths[0]
	}

	fmt.Fprintln(output, contentLength)
	w.Write([]byte("{}"))
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
	http.ListenAndServe(":8126", nil)
}
