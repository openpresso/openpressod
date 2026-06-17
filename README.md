# Openpressod

[![CI Status](https://github.com/openpresso/openpressod/actions/workflows/ci.yml/badge.svg)](https://github.com/openpresso/openpressod/actions/workflows/ci.yml)
[![Publish release](https://github.com/openpresso/openpressod/actions/workflows/release-publish.yml/badge.svg?event=release)](https://github.com/openpresso/openpressod/actions/workflows/release-publish.yml)
[![GPLv3](https://img.shields.io/badge/license-%20%20GNU%20GPLv3%20-green)](LICENSE)
![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg?logo=c%2B%2B)
![Platform](https://img.shields.io/badge/Platform-Linux-lightgrey.svg?logo=linux)
[![Cloudsmith](https://img.shields.io/badge/OSS%20hosting%20by-cloudsmith-blue?logo=cloudsmith&link=https%3A%2F%2Fcloudsmith.com)](https://cloudsmith.com)

**Openpressod** is the hardware control daemon of the [OpenPresso Project](https://openpresso.org). 

It utilizes the underlying [libopenpresso](https://github.com/openpresso/libopenpresso) C++ library to configure it specificly for espresso machines with hardware architecture similar to **Gaggia Classic Pro** and manages all interactions with client services. The daemon exposes the machine's control and monitoring capabilities over a gRPC interface.

## Documentation & Resources

For detailed information on installation, configuration, and usage, please refer to the following resources:

- 🏠 **[OpenPresso Main Site](https://openpresso.org)**: General project overview and ecosystem.
- 📖 **[Technical Documentation](https://openpresso.org/openpresso-docs/openpressod)**: Detailed installation, configuration and usage guides.

---

## Packages hosting

Package repository hosting is graciously provided by [Cloudsmith](https://cloudsmith.com).
Cloudsmith is the only fully hosted, cloud-native, universal package management solution, that
enables your organization to create, store and share packages in any format, to any place, with total
confidence.

---

## License

Openpressod is licensed under the **GNU General Public License v3.0**. See the [LICENSE](LICENSE) file for more information.
