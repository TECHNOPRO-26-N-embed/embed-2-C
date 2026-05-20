# embed-1-{グループID} — 組込み開発実習リポジトリ

**テクノプロ・デザイン 2026年度 組込み開発実習 Class 1**

---

## 📁 フォルダ構成

```
embed-1-{グループID}/
├── members/
│   └── {your-github-id}/        ← 自分のGitHub IDでフォルダを作成
│       ├── morning_challenge/   朝チャレ提出ファイル
│       ├── docs/                要件定義書・設計書
│       └── src/                 ソースコード
├── shared/                      グループ全員で使う共有スペース
└── templates/                   ← 講師が配布するテンプレート置き場
    ├── requirements_template.md 要件定義書テンプレート
    ├── member_setup_guide.md    初回セットアップ手順（受講生向け）
    └── org_invitation_guide.md  Org招待手順（講師向け）
```

---

## 🚀 初回セットアップ（初日にやること）

1. このリポジトリをクローン
   ```bash
   git clone https://github.com/TECHNOPRO-26-N-embed/embed-1-{グループID}.git
   ```

2. **自分のフォルダを作成**（`your-github-id` を自分の GitHub ID に変える）
   ```bash
   cd embed-1-{グループID}
   cp -r members/your-github-id members/{自分のGitHub ID}
   git add .
   git commit -m "init: {自分のGitHub ID} の作業フォルダを作成"
   git push
   ```

---

## 📋 各フォルダの使い方

### `members/{github-id}/morning_challenge/`
朝チャレのJSONファイルをここに置いて push してください。
```
result_260521.json
result_260522.json
...
```

### `members/{github-id}/docs/`
要件定義書・設計書を置きます。

### `members/{github-id}/src/`
実習のソースコードを置きます。

### `shared/`
グループ全員で共有したいファイル（メモ・参考コード等）を置いてください。

### `templates/`
講師が配布するテンプレートが置かれます。**編集せず**、自分の `docs/` にコピーして使ってください。
```bash
# テンプレートを自分のdocsフォルダにコピーする例
cp templates/requirements_template.md members/{自分のGitHub ID}/docs/requirements.md
```

---

## ⚠️ ルール

- **他の人のフォルダは編集しない**
- `main` ブランチへの直接 push は OK（慣れてきたらブランチ運用も練習します）
- コミットメッセージは日本語でもOK
