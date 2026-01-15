# DuckDB - Spark Connect Extension
## Introduction
A project to build DuckDB extension to connect to Spark via Spark Connect API - so DuckDB can read/write data via Spark.

## Design

### Overview
```mermaid
flowchart TD

spark_connect(Spark Connect Driver)
duckdb_ext(DuckDB Spark Extension with gRPC and Arrow)
duckdb(DuckDB)

spark_connect <-- Spark Connect API --> duckdb_ext <-- DuckDB Extension API --> duckdb
```

### Read path

```mermaid
flowchart TD

storage(Data Storage)
catalog(Data Catalog)
spark_connect(Spark Connect Driver)
duckdb_ext(DuckDB Extension with grpc and arrow)
duckdb(DuckDB)

catalog -- get tables metadata --> spark_connect
storage -- read tables as DataFrame --> spark_connect -- send table data as Arrow batches via gRPC --> duckdb_ext -- read data as arrow table --> duckdb
```

### Write path

```mermaid
flowchart TD

storage(Data Storage)
catalog(Data Catalog)
spark_connect(Spark Connect Driver)
duckdb_ext(DuckDB Extension with grpc and arrow)
duckdb(DuckDB)

duckdb -- write data as arrow table --> duckdb_ext -- write data as arrow batch via gRPC --> spark_connect -- write data from DataFrame to tables --> storage
spark_connect -- update table metadata --> catalog
```

## Setup VSCode Development Environment
1. Install VSCode extensions:
- clangd: `llvm-vs-code-extensions.vscode-clangd`
- c/c++ extension pack: `ms-vscode.cpptools-extension-pack`

2. Setup `clangd`:
- after run `make` successfully, run script to tell clangd how your project is built (by create a link to built `compile_commands.json` file).

```bash
ln -s build/release/compile_commands.json compile_commands.json
```

3. Enjoy development.
## Documentation
- [Extension Docs](./docs/README.md)
