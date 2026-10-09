# Publishing this sanitized source snapshot

This source archive has **not been committed or published**. Create a **private** empty repository named `Phantasy-Star-Collection-GBA` on your own GitHub account, extract this ZIP, and execute:

```bash
cd Phantasy-Star-Collection-GBA
git init
git branch -M main
git add .
git status --short
git commit -m "Import sanitized PSC R48 CP58 source"
git remote add origin https://github.com/YOUR_GITHUB_USERNAME/Phantasy-Star-Collection-GBA.git
git push -u origin main
```

Only source files/docs are staged. Do not force-add ignored binary ROMs, game-derived assets, native test executables, or old toolchain packages. This project incorporates reverse-engineered third-party game functionality and is recommended to remain private until redistribution rights are checked.
