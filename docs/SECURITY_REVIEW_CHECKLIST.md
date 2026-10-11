# Security Review Checklist (Upgrade)

Use this as the Phase 12 gate for each merge into `master`.

## Architecture
- [x] Changes preserve Arqma naming / public API identity
- [x] New modules ship with docs + unit tests where applicable
- [ ] Dual serialization stacks reduced (tracked, not complete)

## Security
- [x] HF19 burn gate aligned wallet/core
- [x] Arq-Net unknown peers denied
- [x] RPC batch caps for get_transactions / is_key_image_spent
- [x] Pagination clamps for service node listing
- [ ] Full fuzz corpus in CI
- [ ] Tor/I2P anonymity network threat model revalidated

## Compatibility
- [x] Additive RPC preferred over breaking changes
- [x] Arq-Net ping not hard-gating uptime proofs yet
- [ ] Operator release notes published for any future hard gate

## Performance
- [x] Baseline measurement doc added
- [ ] IBD / startup profiles on CI hardware

## Tests / CI
- [x] Curated unit suite green
- [x] Native CI + sanitizer workflow present
- [ ] Sanitizer job green on GitHub runners
- [ ] Legacy fixture restoration complete

## Release
- [ ] Tag candidate build artifacts from depends CI
- [ ] Signed release notes
- [ ] Stagenet soak with SN + Storage Server
