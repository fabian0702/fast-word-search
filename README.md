# Fast indexed text search

THere is a http api on port 8000 (Endpoints listed below)

## Endpoints:

- /build/ -> Build the index based on the file give in the docker command

- /search/unverified?query=... -> launch a query and get back a list of line numbers where the query is likely part (fastest)
- /search/verified?query=... -> launch a query and get back a list of line numbers where the query is definitely part (fast)
- /search/results?query=... -> launch a query and get back the lines where the query is definitely part (slowest)

## Building
Depending on filesize it might take multiple minutes (~10min at 4GB of text). The final index for a ~4GB file of text will be ~18GB and building the index required ~50GB of free disk space. To add more content add the content to the file and trigger a rebuild with /build/