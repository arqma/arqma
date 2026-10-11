# Security Notes (Upgrade Branch)

## Fixes landed on `upgrade`

1. **HF19 burn construction gate**  
   `construct_tx` previously rejected burns when `hard_fork_version <= 19`,
   contradicting wallet rules (`>= 19`) and the error message (“before HF19”).
   Fixed to `< network_version_19`.

2. **Arq-Net authentication surface**  
   Unknown Curve25519 peers were accepted as `allow::client`, exposing the ZMQ
   listener to unauthenticated clients. Unknown identities are now
   `allow::denied`; only registered service nodes are accepted.

3. **Arq-Net ping visibility**  
   Restored `arqnet_ping` RPC, `m_last_arqnet_ping`, and info reporting so
   operators can monitor reachability. Uptime-proof hard-fail remains disabled
   until operator tooling and release notes are ready (compatibility).

## Ongoing risks

| Risk | Mitigation plan |
|------|-----------------|
| Missing CLSAG/HF19 core_tests | Unit CLSAG added; chaingen coverage next |
| Monolithic `wallet2` / LMDB files | Incremental extraction, not big-bang rewrite |
| Dual serialization stacks | Prefer epee/MQ path documentation + gradual reduction |
| RPC SSL `autodetect` | Document operator hardening; consider safer defaults later |
| ZMQ RPC auth = bind trust | Keep localhost default; document exposure |
| Large P2P packet limits | Inherited; revisit with DoS budget analysis |
| Ubuntu 20.04 depends CI image | Migrate depends jobs toward 22.04/24.04 |

## Engineering rules

- Validate all network deserialization boundaries.
- Prefer RAII; no raw owning pointers in new code.
- Never trade cryptographic/consensus safety for micro-optimizations.
- Preserve licenses and copyright headers when adapting external code.
- Do not add Co-Authored-By trailers or non-required contributor metadata.
