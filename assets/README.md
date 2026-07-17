# Midnight assets

Source assets are tracked under directories named for their runtime role.
Third-party asset packs must keep a `SOURCE.md` beside their files with the
creator, source URL, license, and import details.

Files in this tree are development inputs. The build copies the runtime asset
set into `build/assets`. Generated or transformed assets belong in
`assets_cooked`, which is intentionally ignored by Git.

Do not edit imported third-party originals in place. Put project-authored
derivatives in a separate directory and document how they were produced.
