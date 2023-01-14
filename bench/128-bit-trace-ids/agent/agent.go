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

func content(w http.ResponseWriter, req *http.Request) {
	contentLength := "0"

	lengths := req.Header["Content-Length"]
	if lengths != nil {
		if len(lengths) != 1 {
			panic(fmt.Sprintf("Content-Length has %d separate values: %v", len(lengths), lengths))
		}
		contentLength = lengths[0]
	}

	_, err := fmt.Fprintln(output, contentLength)
	if err != nil {
		panic(fmt.Sprintf("Unable to write to output: %v", err))
	}

	w.Write([]byte("{}"))
}

func main() {
	path, ok := os.LookupEnv("BENCH_OUTPUT")
	if !ok {
		panic("Missing BENCH_OUTPUT environment variable")
	}
	fmt.Println("Going to create/append file:", path)

	outputFile, err := os.Create(path)
	if err != nil {
		panic(fmt.Sprintf("Unable to create/append %v because %v", path, err))
	}
	output = bufio.NewWriter(outputFile)
	defer func() {
		fmt.Println("Deferred flush and close commencing...")
		output.Flush()
		outputFile.Close()
	}()

	signals := make(chan os.Signal, 1)
	signal.Notify(signals, syscall.SIGINT, syscall.SIGTERM)
	go func() {
		<-signals
		fmt.Println("I received a signal.")
		output.Flush()
		outputFile.Close()
		os.Exit(0)
	}()

	http.HandleFunc("/", content)
	http.ListenAndServe(":8126", nil)
}
