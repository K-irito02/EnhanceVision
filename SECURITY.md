# Security Policy

English | [简体中文](SECURITY_CN.md)

## Supported Versions

The following versions are currently receiving security updates:

| Version | Supported |
| ------- | --------- |
| 0.1.x   | ✅ Yes    |
| < 0.1.0 | ❌ No     |

## Reporting a Vulnerability

We take the security of EnhanceVision seriously. If you discover a security vulnerability, please **do not** report it through public GitHub Issues.

### Reporting Methods

Please report security vulnerabilities privately through:

1. **GitHub Security Advisories** (Recommended)
   - Visit [Security Advisories](https://github.com/K-irito02/EnhanceVision/security/advisories)
   - Click "Report a vulnerability"
   - Fill in the details

2. **Email**
   - Send to: saokiiritoasuna@qq.com
   - Subject: `[Security] EnhanceVision Vulnerability Report`

### What to Include

Please include the following information in your report:

- Vulnerability type (e.g., buffer overflow, XSS, privilege escalation)
- Affected versions
- Steps to reproduce
- Proof of concept code (if applicable)
- Impact assessment
- Possible fix suggestions (optional)

### Response Timeline

| Stage | Expected Time |
|-------|---------------|
| Acknowledge report | Within 48 hours |
| Initial assessment | Within 7 days |
| Fix development | Depends on severity |
| Patch release | Within 7 days after fix |

### Disclosure Policy

- Do not publicly disclose the vulnerability before a patch is released
- We will publicly acknowledge your contribution after the fix is released (if you wish)
- For critical vulnerabilities, we will publish a security advisory

## Security Best Practices

### For Users

1. **Download Sources**
   - Only download EnhanceVision from official channels
   - Verify the integrity of downloaded files

2. **AI Models**
   - Only use AI models from trusted sources
   - Place model files in the application's `models/` folder

3. **File Handling**
   - Be cautious when processing files from untrusted sources
   - Regularly backup important files

### For Developers

1. **Dependency Updates**
   - Regularly update third-party dependencies
   - Monitor security advisories for dependencies

2. **Code Review**
   - All code changes must be reviewed
   - Pay special attention to memory safety and input validation

3. **Sensitive Information**
   - Do not hardcode sensitive information in code
   - Do not commit sensitive information in logs or config files

## Known Security Limitations

1. **Local Processing**
   - EnhanceVision runs entirely locally and does not send any data to external servers
   - User data never leaves the local machine

2. **AI Models**
   - AI models are from third-party projects, ensure you obtain them from official sources
   - Model files may pose potential risks, handle with caution

## Acknowledgments

We thank the following security researchers for their contributions:

<!-- Add security researcher acknowledgments here -->

---

Thank you for helping protect EnhanceVision and its users!
