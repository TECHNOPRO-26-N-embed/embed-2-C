# テンプレート置き場

講師が配布するテンプレートはここに置かれます。
自分の `members/{GitHub ID}/docs/` にコピーして使ってください。

## テンプレート一覧

| ファイル | 内容 | 使うタイミング |
|:---|:---|:---|
| `requirements_template.md` | 要件定義書テンプレート | Day 1（初日） |
| `basic_design_template.md` | 基本設計書テンプレート | Day 2〜3（本日〜） |
| `detailed_design_template.md` | 詳細設計書テンプレート | Day 2後半〜Day 3 |

## 使い方

```bash
# 基本設計書テンプレートをコピー（本日〜）
cp templates/basic_design_template.md members/{自分のGitHub ID}/docs/basic_design.md

# 詳細設計書テンプレートをコピー（基本設計完了後）
cp templates/detailed_design_template.md members/{自分のGitHub ID}/docs/detailed_design.md

# 編集してcommit
git add members/{自分のGitHub ID}/docs/
git commit -m "基本設計書 第1稿"
git push
```
