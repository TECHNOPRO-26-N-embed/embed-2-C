# グループリポジトリ セットアップガイド

講師向け：受講生をOrganizationに招待する手順

---

## ① Organization に受講生を招待する

### 方法A：GitHub Web UI（少人数向け）

1. https://github.com/orgs/TECHNOPRO-26-N-embed/people にアクセス
2. 「Invite member」をクリック
3. 受講生の **GitHub ID** を入力 → 招待送信
4. 受講生が招待メールを承認するとメンバーになる

### 方法B：gh CLI（一括招待）

受講生のGitHub IDを `members.txt` に1行ずつ記載してから：

```bash
# members.txt の例:
# yamada-taro
# tanaka-hanako
# ...

while read id; do
  gh api orgs/TECHNOPRO-26-N-embed/invitations \
    -f invitee_login="$id" \
    -f role=direct_member
  echo "招待送信: $id"
done < members.txt
```

---

## ② リポジトリへのアクセス権限

Organization メンバーになれば、`embed-1-A`〜`embed-1-L` の **Public リポジトリ** は自動的に見られます。

**Push権限が必要な場合**（Privateリポジトリにする場合）は、各リポジトリのCollaboratorに追加が必要：

```bash
# グループAのメンバー全員をCollaboratorに追加する場合
gh api repos/TECHNOPRO-26-N-embed/embed-1-A/collaborators/yamada-taro \
  -X PUT -f permission=push
```

---

## ③ 受講生への案内文（Slack）

```
【GitHub Organizationへの招待について】

メールアドレス（GitHub登録メール）宛にOrganization招待が届きます。
承認するとグループリポジトリにアクセスできるようになります。

グループリポジトリURL：
  https://github.com/TECHNOPRO-26-N-embed/embed-1-{あなたのグループID}

招待メールが届かない場合はSlackでお知らせください。
```

---

## ④ 受講生のGitHub IDの収集方法

朝チャレHTMLのGitHub ID入力 → result_DATE.json を回収すればIDが収集できます。

または：Google Form で事前収集が最もシンプルです。
