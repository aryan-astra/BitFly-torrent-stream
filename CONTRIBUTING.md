# Contributing

Use C++20, Qt 6.8-compatible APIs and the pinned dependency baseline. Keep torrent
and HTTP work off the GUI thread, preserve ByteSource's verified-only contract,
and never introduce shell execution or automatic cache deletion. Changes need
focused deterministic tests and a concise description of user-visible behavior.

Run the checks in docs/TESTING.md before submitting a pull request. Use generated
or openly licensed media, never private or unauthorized downloads. UI changes
need actual rendered screenshots at compact sizes and multiple DPI scales.

All human-authored project commits must use the contributor's authorized signing
identity. Never bypass hooks, disable signing or add private signing keys to CI.
Use conventional commit messages. Report security defects privately according
to SECURITY.md. Contributions are under the MIT license.
