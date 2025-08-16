ARTS Visualizer

CLI: carts-visualizer

Usage examples:

```bash
carts visualize test/tasksimple/simple.mlir --func main --json -o graph.json
carts visualize test/tasksimple/simple.mlir --func main -o graph.dot
carts visualize test/tasksimple/simple.mlir --func main --concurrency -o conc.dot
```

JSON schema (abridged):

```json
{
  "db": { "nodes": [{"id": "A"}], "edges": [{"from":"A","to":"A.1","type":"Lifetime"}] },
  "edt": { "nodes": [{"id":"T1"}], "edges": [{"from":"T1","to":"T2","type":"Dep"}] },
  "concurrency": { "edges": [{"from":"T1","to":"T2","kind":"AliasConflict"}] }
}
```

Integrates with `ArtsGraphManager` to ensure consistency with analysis traits and queries.

