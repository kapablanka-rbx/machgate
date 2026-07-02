# Release Workflow

When a user asks for a MachGate release, do not stop after building a local
`dist/` tarball. A release means publishing GitHub release assets from the
repository release workflow.

The supported flow is tag-driven:

1. Commit the intended source, test, and documentation changes.
2. Push the commit to `kapabl/machgate`.
3. Create and push an annotated version tag, for example `v0.3.53`.
4. Let `.github/workflows/release.yml` run on `ubuntu-24.04-arm`.
5. Actively watch the workflow until it finishes and publishes the GitHub
   release.
6. Notify the user immediately with the GitHub release URL and asset names.

The workflow builds and tests the ARM64 release in Docker, stages the package,
creates:

- `machgate-<version>-linux-arm64.tar.gz`
- `machgate-<version>-linux-arm64.tar.gz.sha256`

and uploads both as GitHub release assets through
`softprops/action-gh-release`.

Local `dist/` artifacts are only smoke-test inputs. They are not downloadable by
the user through `scripts/run-macho-docker.sh` unless they are uploaded by the
GitHub release workflow.

Do not make the user poll for release status. After pushing the version tag,
watch the release workflow with `gh run watch` or repeated `gh run view` checks.
When it succeeds, report the workflow URL, release URL, and uploaded asset
names. When it fails, report the failed workflow URL and the failing step or log
summary; do not describe the release as available until GitHub has published the
assets.

Useful commands:

```bash
git push origin master
git tag -a vX.Y.Z -m "MachGate vX.Y.Z"
git push origin vX.Y.Z
gh run list -R kapabl/machgate --workflow release.yml --limit 5
gh run watch -R kapabl/machgate <run-id>
gh release view vX.Y.Z -R kapabl/machgate --json url,assets
```

Never manually upload a locally built tarball as the first choice. Use manual
upload only to repair a failed release workflow after identifying the workflow
failure and confirming that the local artifact was built from the exact tagged
commit.
