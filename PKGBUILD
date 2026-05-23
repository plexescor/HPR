pkgname=hpr
pkgver=0.4
pkgrel=1
pkgdesc="Offline zero-account activity tracker"
arch=('x86_64')
url="https://github.com/plexescor/HPR"
license=('GPL')

depends=('glibc')
makedepends=('cmake' 'git')

source=("$pkgname-$pkgver.tar.gz::$url/archive/refs/tags/v$pkgver.tar.gz")
sha256sums=('SKIP')

build() {
    cd "HPR-$pkgver"

    chmod +x installDependencies.sh
    ./installDependencies.sh

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build
}

package() {
    cd "HPR-$pkgver/build"

    chmod +x installHPRConfigAndUi.sh
    sudo ./installHPRConfigAndUi.sh

    install -Dm755 HPR "$pkgdir/usr/bin/hpr"

    install -Dm644 ../LICENSE.txt \
        "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
