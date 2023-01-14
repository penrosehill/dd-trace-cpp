Run `bin/bench` to build the benchmark, and then run the following to run it:
```
docker compose up --build --abort-on-container-exit --remove-orphans
```
All of the output is under `output/`.

Here are the configurations:

- `baseline`: No tracing, just make an HTTP request and wait for the response.
- `control`: Tracing with 64-bit trace IDs.
- `test`: Tracing with 128-bit trace IDs.

