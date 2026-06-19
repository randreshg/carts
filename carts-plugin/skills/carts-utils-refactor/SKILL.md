---
name: carts-utils-refactor
description: Use when searching for existing CARTS helpers, or before adding new utilities, passes, or attributes.
---

# CARTS Utilities & Refactoring

- **Search Before Writing**: Before adding a new helper, use `grep_search` in `include/carts/utils/` and `lib/carts/utils/` to ensure it doesn't already exist.
- **Scope**: Keep pass-local helpers truly local unless they are needed globally. Put shared behavior in the narrowest semantic owner.
- **Attributes**: Do not hardcode attribute names. Look up existing attributes in `include/carts/IR/CommonAttrs.td` and dialect `*.td` files.
