# verify/ -- Pipeline Verification Passes

Lightweight assertion passes inserted between pipeline stages to catch
invariant violations early. Each verifier checks that a specific
transformation has (or has not) been applied.

| Pass | Checks |
|------|--------|
| VerifyMetadata | Required metadata attributes are present |
| VerifyMetadataIntegrity | Metadata is internally consistent |
| VerifyEdtCreated | EDT operations exist after OMP conversion |
| VerifyEpochCreated | Epoch barriers exist after CreateEpochs |
| VerifyForLowered | Distributed for-loops have been lowered |
| VerifyDbCreated | DataBlock allocations exist after CreateDbs |
| VerifyPreLowered | Pre-lowering invariants are satisfied |
| VerifyEdtLowered | EDT operations have been lowered to runtime calls |
| VerifyDbLowered | DB operations have been lowered to runtime calls |
| VerifyEpochLowered | Epoch operations have been lowered to runtime calls |
| VerifyLowered | All ARTS operations have been fully lowered |
