# Lumen

App de filmes e séries para Android TV (TCL 55P7K), feito a partir do Stremio Origin.
Usa os addons do Stremio, TMDB e Trakt.

## Como atualizar

Cada etapa chega como um patch:

```
cd ~ && unzip -o ~/storage/downloads/etapa-N.zip -d ~ && atualizar-origin ~/etapa-N.patch
```

O GitHub compila o APK (`.github/workflows/build-atv-debug.yml`), publica uma release
e a TV instala (por adb, ou sozinha em Configurações > Sistema > Atualizações e ajuda).

## Pastas

- `src/` — o app (C, SDL2/GLES2)
- `androidtv/` — a parte Android (Java, ExoPlayer)
- `assets/` — traduções, ícones, fontes e sons
- `tools/` — scripts de build
- `tests/` — testes das partes centrais

Licença: GPL-3.0 (veja `LICENSE`).
