# GLaDOS voices

The complete [release package](https://github.com/Crafitys/Crafitys_Glados/releases/latest/download/Crafitys_Glados.zip) contains both ONNX voices and their JSON files.

The Git source tree keeps the small JSON configurations. If you cloned the repository or used **Code → Download ZIP**, install the matching ONNX files with:

```bash
python tools/download_voices.py
```

The installer reads only the expected model files from the release and verifies their SHA-256 checksums. You can instead manually copy the two `.onnx` files from the complete release archive into this folder.

See `ASSET_NOTICES.md` for the supplied voice provenance information.
