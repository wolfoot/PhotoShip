[简体中文](README.zh_CN.md) · [English](README.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [Français](README.fr.md) · [Deutsch](README.de.md) · [Español](README.es.md)

# PhotoShip

<p align="center">
  <img src="packaging/icons/photoship-128.png" width="96" height="96" alt="PhotoShip — Layer Sail">
</p>

**Ubuntu와 Windows용 경량 레이어 이미지 편집기.**

Qt 6와 C++17로 만든 PhotoShip은 브러시, 선택 영역, 레이어 마스크, 비파괴 조정과 편집 가능한 프로젝트를 제공합니다. 기본 이미지 편집, 합성과 그래픽 제작에 사용할 수 있습니다. 앱 코드는 [MIT 라이선스](LICENSE)로 제공됩니다.

[빌드 워크플로](https://github.com/wolfoot/PhotoShip/actions/workflows/build.yml) · [문제 보고](https://github.com/wolfoot/PhotoShip/issues) · [제삼자 라이선스](THIRD_PARTY_NOTICES.md)

![PhotoShip — 영어 인터페이스](docs/preview.png)

## 기능

| 분류 | 지원 기능 |
| --- | --- |
| 문서와 레이어 | 문서 탭, 중첩 그룹, 다중 선택, 드래그 정렬, 일괄 복제·삭제, 인접 레이어 병합 |
| 그리기와 보정 | 브러시, 지우개, 스포이트, 태블릿 필압, 복제 도장과 기본 복구 |
| 선택과 변형 | 사각·타원 선택, 올가미, 연속 영역 마술봉, 선택 픽셀 이동·복사, 이동, 크기 조절, 회전, 뒤집기, 자르기 |
| 합성 | 혼합 모드 12종, 불투명도, 래스터 마스크, 클리핑 마스크, 편집 가능한 텍스트와 사각·타원 도형 |
| 조정 | 레벨, RGB 곡선, 색조·채도·밝기, 반전. 조정 레이어 또는 직접 픽셀 편집 |
| 파일 | PNG/JPEG 가져오기·내보내기, 기본 PSD와 Compositor `.comp` 가져오기, 편집 가능한 `.psproj` |
| 작업 흐름 | 실행 취소·다시 실행, 백그라운드 저장·내보내기, 자동 복구, 클립보드, 드래그 가져오기, 뷰포트 타일 캐시 |
| 언어 | 중국어 간체, 영어, 일본어, 한국어, 프랑스어, 독일어, 스페인어. 즉시 전환 및 설정 저장 |

현재 버전: **0.2.1**. Ubuntu에서 로컬 빌드, 자동 검사와 X11 실행 검증을 완료했습니다. Windows 빌드 및 패키징 스크립트는 제공되지만 Windows 환경에서 검증이 필요합니다. 필압 검사는 시뮬레이션 이벤트를 사용하며 실제 하드웨어 호환성은 장치와 드라이버에 따라 달라집니다.

## 다운로드와 설치

### Ubuntu

빌드 기준: Ubuntu 22.04/24.04 x86_64, Qt 6.2 이상, CMake 3.21 이상, C++17 컴파일러.

```bash
git clone https://github.com/wolfoot/PhotoShip.git
cd PhotoShip
sudo apt update
sudo apt install qt6-base-dev qt6-qpa-plugins zlib1g-dev cmake ninja-build g++
./scripts/build-linux.sh
```

스크립트는 빌드와 검사를 실행하고 `dist/photoship-0.2.1-Linux.deb`를 생성합니다:

```bash
sudo apt install ./dist/photoship-0.2.1-Linux.deb
photoship
```

DEB는 시스템 Qt에 동적 링크되며 apt가 필요한 의존성을 설치합니다. 중국어·일본어·한국어 글꼴이 없으면 `fonts-noto-cjk`를 설치하세요. 오프라인 설치에는 런타임 라이브러리와 글꼴을 미리 준비해야 합니다.

개발 중에는 빌드 디렉터리에서 직접 실행할 수 있습니다:

```bash
./scripts/run.sh
./scripts/run.sh --demo
./scripts/run.sh /absolute/path/to/project.psproj
```

### Windows

대상: Windows 10/11 x64. 다음을 준비하세요:

- Visual Studio 2022의 **Desktop development with C++** 워크로드.
- CMake와 Qt 6.8.3의 **MSVC 2022 64-bit** 구성 요소.
- PSD ZIP 디코딩용 vcpkg와 `zlib:x64-windows-static-md`.
- Inno Setup 6. 설치 프로그램을 만들 때만 필요합니다.

프로젝트 디렉터리에서 PowerShell을 실행하세요:

```powershell
vcpkg install zlib:x64-windows-static-md
./scripts/build-windows.ps1 -QtPrefix 'C:\Qt\6.8.3\msvc2022_64' -ZlibToolchain 'C:\vcpkg\scripts\buildsystems\vcpkg.cmake'
```

Qt 런타임이 포함된 휴대용 패키지 `dist/PhotoShip-0.2.1-windows-x64.zip`를 생성합니다. 압축을 풀고 `photoship.exe`를 실행하세요. `-Installer`를 추가하면 `dist/PhotoShip-0.2.1-windows-x64-setup.exe`를 생성하며 기본적으로 현재 사용자 디렉터리에 설치합니다.

저장소의 [GitHub Actions 워크플로](.github/workflows/build.yml)는 두 플랫폼의 빌드, 검사와 결과물 업로드를 구성합니다. 성공한 실행에서 결과물을 다운로드할 수 있습니다. 워크플로 설정만으로 해당 플랫폼 검증이 완료된 것은 아닙니다.

## 사용 방법

1. 문서를 만들거나 이미지를 열고 레이어 패널에서 편집할 레이어를 선택하세요.
2. 왼쪽 도구 모음으로 그리기, 선택과 변형을 수행하세요. 오른쪽 속성 패널에서 정확한 변형 값을 입력할 수 있습니다.
3. 레이어 메뉴에서 마스크, 클리핑과 병합을 사용하고 이미지 메뉴에서 조정 레이어를 만드세요.
4. **프로젝트 저장**으로 편집 가능한 내용을 보존하고 **PNG / JPEG 내보내기**로 병합 이미지를 출력하세요.

복제·복구 도구는 같은 래스터 레이어에서 **Alt 클릭**으로 원본을 선택한 후 그립니다. 복구는 로컬 RGB 색조 일치를 사용하는 기본 보정입니다. 마스크에서 검정은 숨기고 흰색은 표시합니다.

병합에는 같은 수준의 인접한 일반 혼합 모드 레이어가 필요합니다. 클리핑·조정 의존성이나 반투명 상위 그룹이 있으면 화면 변경을 방지하기 위해 병합이 거부될 수 있습니다. 그룹은 pass-through 합성을 사용하므로 조정 레이어가 그룹 밖의 아래 내용에도 영향을 줍니다.

### 언어

**Language / 语言** 메뉴에서 재시작 없이 언어를 선택하세요. 첫 실행은 시스템 언어를 따르며 미지원 언어는 영어로 대체됩니다. 전환해도 문서 내용, 레이어 이름과 실행 취소 기록은 유지됩니다.

저장된 설정을 바꾸지 않고 이번 실행에만 언어를 지정할 수 있습니다:

```bash
./scripts/run.sh --language ko
# en / zh_CN / ja / ko / fr / de / es
```

메뉴, 패널, 편집 대화 상자와 일반 메시지는 번역되어 있습니다. 일부 파서 및 운영체제 진단은 영어로 표시될 수 있습니다.

### 주요 단축키

| 작업 | 단축키 |
| --- | --- |
| 이동 / 브러시 / 지우개 | V / B / E |
| 복제 / 복구 | S / J, Alt 클릭으로 원본 선택 |
| 사각 / 타원 / 올가미 / 마술봉 | M / Shift+M / L / W |
| 자르기 / 사각형 / 타원형 | C / U / Shift+U |
| 텍스트 / 스포이트 / 손 | T / I / H |
| 화면 이동 / 확대 | Space 드래그 또는 가운데 버튼 / 마우스 휠 |
| 브러시 크기 | [ / ] |
| 실행 취소 / 다시 실행 | Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z |
| 새로 만들기 / 열기 / 저장 / 다른 이름으로 저장 | Ctrl+N / Ctrl+O / Ctrl+S / Ctrl+Shift+S |
| 가져오기 / 내보내기 | Ctrl+Shift+O / Ctrl+Shift+E |
| 레이어 복제 / 선택 레이어 병합 | Ctrl+J / Ctrl+E |
| 클리핑 마스크 전환 | Ctrl+Alt+G |
| 전체 선택 / 선택 해제 | Ctrl+A / Ctrl+D |
| 캔버스 맞추기 / 실제 픽셀 | Ctrl+0 / Ctrl+1 |
| 브러시 획 또는 드래그 취소 | Esc |

## 파일과 데이터 저장

### 편집 가능한 프로젝트

`.psproj`는 `manifest.json`과 `images/`를 포함하는 폴더입니다. 이동하거나 백업할 때 폴더 전체를 유지하세요. 레이어, 그룹, 마스크, 변형, 텍스트와 조정 매개변수를 저장합니다. 선택 영역, 실행 취소 기록과 뷰포트 위치는 저장하지 않습니다. 텍스트는 로컬 글꼴을 사용하므로 다른 컴퓨터에서 글꼴이 대체될 수 있습니다.

저장은 새 자산을 쓰고 목록 파일을 원자적으로 확정하며 파일 잠금으로 동시 쓰기를 방지합니다. 백그라운드 저장은 시작 시점의 스냅샷을 기록하므로 이후 편집은 다시 저장해야 합니다. 이미지 내보내기는 프로젝트를 저장됨으로 표시하지 않습니다.

자동 복구는 편집이 멈춘 약 1.5초 후 스냅샷을 쓰며 30초마다 다시 확인합니다. 시작 시 복구, 버리기 또는 나중에 처리를 선택할 수 있습니다. 복구는 원본 프로젝트를 유지하고 별도의 미저장 문서를 엽니다. 스냅샷 쓰기가 끝나지 않은 편집은 복구되지 않을 수 있습니다.

PhotoShip은 기존 프로젝트, 언어 설정과 복구 스냅샷을 유지하기 위해 이전 Pixel Studio의 형식 식별자 `org.pixelstudio.project`와 저장 위치를 사용합니다. 현재 형식 버전은 2이며 버전 1을 읽을 수 있습니다. `PHOTOSHIP_RECOVERY_DIR`로 복구 디렉터리를 지정할 수 있으며 기존 `PIXELSTUDIO_RECOVERY_DIR`도 지원합니다.

### PSD와 Compositor 가져오기

PSD v1, 8비트 RGB, 래스터 레이어, 그룹, 래스터 마스크와 일부 클리핑 관계를 지원합니다. 채널은 raw, RLE, ZIP 또는 ZIP 예측 압축을 사용할 수 있습니다. 가져오기 전에 호환성 보고서를 표시합니다. 텍스트, 벡터, 스마트 오브젝트, 효과와 조정 매개변수는 캐시 픽셀로 대체되거나 생략될 수 있습니다. PSD 내보내기는 지원하지 않습니다.

기본 `.comp` 가져오기는 래스터 레이어, 그룹, 변형, 지원되는 혼합 모드와 연결된 래스터 마스크를 처리합니다. 지원하지 않는 기능은 오류로 표시됩니다. 원본 파일을 보존하려면 `.psproj`로 저장하세요.

## 현재 제한

- CPU/QPainter로 합성하며 뷰포트는 256×256 타일을 사용합니다. 일부 필터, PSD 분석과 구조 작업은 메인 스레드에서 실행합니다.
- 한 변당 최대 8192픽셀, 캔버스 총 1600만 픽셀. 원본 레이어 총 3200만 픽셀과 별도의 마스크 3200만 픽셀, 최대 256개 레이어. PSD 파일은 최대 256 MiB입니다.
- 실행 취소는 최대 100단계, 기록 픽셀 버퍼 예산은 약 256 MiB입니다. 활성 이미지, 합성 캐시와 백그라운드 스냅샷은 추가 메모리를 사용하며 전체 프로세스의 메모리 한도는 아닙니다.
- 작업 공간은 8비트 sRGB입니다. PSB, RAW, 16비트/CMYK, 펜 경로, 서식 있는 텍스트, 고급 레이어 스타일, 그룹 마스크, 유동화와 AI 배경 제거는 지원하지 않습니다.
- 기본 가져오기·내보내기 형식은 PNG/JPEG이며 추가 형식은 설치된 Qt 플러그인에 따라 달라집니다. PSD 가져오기와 혼합 공식은 다른 편집기와 픽셀 단위 일치를 보장하지 않습니다.

## 개발과 기여

C++17, Qt 6 Widgets/Concurrent와 zlib를 사용하며 앱에는 네트워크 서비스나 계정이 필요하지 않습니다. 주요 디렉터리:

```text
src/                 편집기 UI, 문서 모델, 합성, 프로젝트·PSD 입출력
assets/i18n/         내장 언어 카탈로그
packaging/           데스크톱 항목, 아이콘, 설치 설정과 라이선스
scripts/             빌드, 실행, 아이콘 생성과 X11 실행 검사
tests/               핵심, v2 기능과 다국어 검사
third_party/         원래 라이선스를 유지하는 제삼자 코드
```

일반 빌드 및 검사 명령:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

자동 검사는 합성, 선택, 실행 취소, 프로젝트 입출력, PSD 디코딩, 백그라운드 저장, 복구와 언어 전환을 다룹니다. Xvfb를 설치한 후 X11 창 실행 검사를 수행할 수 있습니다:

```bash
python3 scripts/check-x11.py --language ko
```

독창적인 **Layer Sail** 아이콘의 원본은 `packaging/photoship.svg`이며 PNG 7개 크기와 Windows ICO를 포함합니다. SVG를 편집한 후 `python3 scripts/render-icons.py`로 재생성하세요. Linux librsvg, Cairo와 Python Pillow가 필요합니다. 번역은 `assets/i18n/*.json`에 있으며 수정 후 다시 빌드하세요.

[Issues](https://github.com/wolfoot/PhotoShip/issues)에 재현 단계, 운영체제, Qt 버전과 예제 파일을 첨부해 보고하세요. 수정, 번역과 기능의 Pull Request를 환영합니다. 기존 파일 형식 호환성을 유지하고 관련 검사를 실행하세요. 제출하는 프로젝트, 스크린샷과 로그에서 개인정보를 제거하세요.

## 라이선스와 감사

PhotoShip 앱 코드는 [MIT 라이선스](LICENSE)입니다. 레이어 작업 흐름은 Compositor를 참고하며 MIT 라이선스의 마술봉 및 경계 추적 코드를 재사용합니다. 원래 저작권과 라이선스는 [third_party/compositor/LICENSE](third_party/compositor/LICENSE)에 유지됩니다.

Qt, Qt 플러그인과 zlib에는 각각의 라이선스가 있으며 앱의 MIT 라이선스는 이를 대체하지 않습니다. 배포 시 라이선스 고지와 요구사항을 준수하세요. [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)를 참고하세요.

PhotoShip은 Adobe와 관련 없는 독립 프로젝트이며 Photoshop 코드나 자산을 포함하지 않습니다.
