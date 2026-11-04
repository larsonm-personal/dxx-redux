# Repair XL reusable secret forwarding

- Confirm all four failed PR controllers reached the trusted retail-ci environment and failed before private checkout because the token input was empty
- Validate GitHub's current documented requirement to forward environment secret names to reusable workflows; preserve environment precedence and trust gates
- Forward only RETAIL_ASSETS_READ_TOKEN, declare the contract, and check only its availability after admission; expose no token value beyond checkout
- Test forwarding, unchanged trust/environment gates, and the exact missing-token shell diagnostic; run scoped lint and public CI
- Keep the unrelated PR #333 hosted disk-pressure failure separate; do not lower reserves, modify credentials/settings, merge, or push cmake
