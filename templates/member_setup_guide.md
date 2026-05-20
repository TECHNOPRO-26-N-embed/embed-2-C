# 初回セットアップ手順（受講生向け）

このリポジトリへの参加初日に実施してください。

---

## ① リポジトリをクローンする

```bash
git clone https://github.com/TECHNOPRO-26-N-embed/embed-1-{グループID}.git
cd embed-1-{グループID}
```

> **グループID** は講師から案内されます（A〜L）

---

## ② 自分のフォルダを作成する

`your-github-id` を **あなたの GitHub ID** に変えて実行してください。

```bash
# フォルダをコピーして自分用に作成
cp -r members/your-github-id members/{あなたのGitHub ID}
```

Windowsのコマンドプロンプトの場合：
```cmd
xcopy members\your-github-id members\{あなたのGitHub ID}\ /E /I
```

---

## ③ テンプレートを自分のdocsフォルダにコピーする

```bash
cp templates/requirements_template.md members/{あなたのGitHub ID}/docs/requirements.md
```

---

## ④ 初回コミット＆プッシュ

```bash
git add members/{あなたのGitHub ID}/
git commit -m "init: {あなたのGitHub ID} フォルダを作成"
git push
```

---

## ⑤ 完成したフォルダ構成の確認

```
embed-1-{グループID}/
├── members/
│   ├── your-github-id/    ← テンプレート（触らなくてOK）
│   └── {あなたのGitHub ID}/   ← ✅ あなたの作業フォルダ
│       ├── morning_challenge/   朝チャレ提出先
│       ├── docs/
│       │   └── requirements.md  ← 要件定義書
│       └── src/                 ← ソースコード置き場
├── shared/                  グループ共有スペース
└── templates/               テンプレート置き場（講師から配布）
```

---

## ❓ よくある質問

**Q: push できない**
→ Organizationの招待を承認しましたか？メールを確認してください。

**Q: `your-github-id` フォルダを消してしまった**
→ 講師に連絡してください。テンプレートリポジトリから再コピーします。

**Q: 朝チャレのJSONはどこに置く？**
→ `members/{あなたのGitHub ID}/morning_challenge/result_YYMMDD.json`
