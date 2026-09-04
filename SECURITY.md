# Security policy

Please report security issues privately through GitHub Security Advisories after publication. Do not include proprietary feed captures, credentials, or personally identifiable information.

The parser rejects unsupported types and exact-length mismatches before field access. Mutation testing is a containment check, not a proof of memory safety. The release workflow should also run AddressSanitizer and UndefinedBehaviorSanitizer on a supported runner.
