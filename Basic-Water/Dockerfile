# PS3 homebrew derleme ortami.
#
# Taban: zeldin/ps3dev-docker - onceden derlenmis ps3toolchain + PSL1GHT
# (ppu-gcc 7.2.0, ppu_rules, fself/make_self_npdrm/pkg/sfo araclari).
# Apple Silicon icin arm64 varyanti mevcut: Rosetta olmadan native hizda calisir.
#
# Not: toolchain'i kaynaktan derlemek de denendi (ps3toolchain/toolchain.sh),
# ancak Docker VM disk alani yetmedigi icin GCC asamasinda basarisiz oluyor.
# Hazir imaj hem hizli hem guvenilir; kaynak derleme gerekirse git gecmisine bakin.
FROM zeldin/ps3dev-docker:latest

ENV PS3DEV=/usr/local/ps3dev
ENV PSL1GHT=${PS3DEV}
ENV PATH=${PATH}:${PS3DEV}/bin:${PS3DEV}/ppu/bin:${PS3DEV}/spu/bin

WORKDIR /project
CMD ["make"]
