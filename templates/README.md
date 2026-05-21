# テンプレート置き場

講師が配布するテンプレートはここに置かれます。
自分の `members/{GitHub ID}/docs/` にコピーして使ってください。

## テンプレート一覧

| ファイル | 内容 | 使うタイミング |
|:---|:---|:---|
| `requirements_template.md` | 要件定義書テンプレート | Day 1（初日）✅ 使用済み |
| `basic_design_template.md` | 基本設計書テンプレート | **Day 2〜3（本日〜）** |
| `detailed_design_template.md` | 詳細設計書テンプレート | Day 2後半〜Day 3 |

## 使い方

```bash
cp templates/basic_design_template.md members/{自分のGitHub ID}/docs/basic_design.md
cp templates/detailed_design_template.md members/{自分のGitHub ID}/docs/detailed_design.md
git add members/{自分のGitHub ID}/docs/
git commit -m "基本設計書 第1稿"
git push
```