# 朝チャレ提出フォルダ

朝チャレ（C言語クイズ）の結果JSONファイルをここに置いて push してください。

## ファイル命名規則

```
result_YYMMDD.json
```

例：
- `result_260521.json`
- `result_260522.json`

## 提出手順

1. `quiz_YYMMDD.html` を開いて回答・提出
2. `result_YYMMDD_{github-id}.json` がダウンロードされる
3. このフォルダにファイルを置く（ファイル名から `_{github-id}` 部分は削除してOK）
4. `git add`, `git commit`, `git push`

```bash
git add members/{あなたのID}/morning_challenge/result_260521.json
git commit -m "朝チャレ 260521 提出"
git push
```
