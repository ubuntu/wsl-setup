# Security policy

## Supported versions

`wsl-setup` is distributed via the Ubuntu archive as the [`wsl-setup` package](https://launchpad.net/ubuntu/+source/wsl-setup).

Currently, we provide security updates for supported LTS releases of Ubuntu on WSL.

If you are unsure of the Ubuntu version you are using, please run the following command in a
WSL terminal running your Ubuntu distro:

```bash
lsb_release -a
```

## Reporting a vulnerability

If you discover a security vulnerability within this repository, we encourage
responsible disclosure. Please report any security issues to help us keep
`wsl-setup` and Ubuntu on WSL secure for everyone.

### Private vulnerability reporting

The most straightforward way to report a security vulnerability is through
[GitHub](https://github.com/ubuntu/wsl-setup/security/advisories/new). For detailed
instructions, please review the
[Privately reporting a security vulnerability](https://docs.github.com/en/code-security/security-advisories/guidance-on-reporting-and-writing-information-about-vulnerabilities/privately-reporting-a-security-vulnerability)
documentation. This method enables you to communicate vulnerabilities directly
and confidentially with the `wsl-setup` maintainers.

The project's admins will be notified of the issue and will work with you to
determine whether the issue qualifies as a security issue and, if so, in which
component. We will then handle finding a fix, getting a CVE assigned and
coordinating the release of the fix to the various Linux distributions.

The [Ubuntu Security disclosure and embargo policy](https://ubuntu.com/security/disclosure-policy)
contains more information about what you can expect when you contact us, and what we expect from you.

#### Steps to report a vulnerability on GitHub

1. Go to the [Security Advisories Page](https://github.com/ubuntu/wsl-setup/security/advisories) of the `wsl-setup` repository.
2. Click "Report a Vulnerability"
3. Provide detailed information about the vulnerability, including steps to reproduce, affected versions, and potential impact.

## Security resources

* [Canonical's Security Site](https://ubuntu.com/security)
* [Ubuntu Security disclosure and embargo policy](https://ubuntu.com/security/disclosure-policy)
* [Ubuntu Security Notices](https://ubuntu.com/security/notices)
* [Ubuntu on WSL documentation](https://documentation.ubuntu.com/wsl/en/latest/)

If you have any questions regarding security vulnerabilities, please reach out
to the maintainers through the aforementioned channels.
