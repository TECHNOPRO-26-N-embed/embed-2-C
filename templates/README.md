# テンプレート置き場

講師が配布するテンプレートはここに置かれます。
自分の `members/{GitHub ID}/docs/` にコピーして使ってください。

## 今日のテンプレート

| ファイル | 内容 |
|:---|:---|
| `requirements_template.md` | 要件定義書テンプレート |

## 使い方

```bash
# テンプレートを自分のdocsフォルダにコピー
cp templates/requirements_template.md members/{自分のGitHub ID}/docs/requirements.md

# 編集してcommit
git add members/{自分のGitHub ID}/docs/requirements.md
git commit -m "要件定義書 第1稿"
git push
```
