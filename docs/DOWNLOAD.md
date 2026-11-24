# 源码下载（SourceForge 慢速/断流应对）

Qt Extended 4.4.3 源码包在 SourceForge：

```
https://downloads.sourceforge.net/project/qpe/QPE/qtopia/qt-extended-opensource-src-4.4.3.tar.gz
```

- 文件大小：190054400 字节（约 181 MiB）
- SHA-256：`6878deed374f37c86214da728945c8b1402a0e3d2c722368f70e62d050cd6d87`

## 问题

- 单连接实测约 65 KB/s，181 MiB 需约 45 分钟；
- 大文件经常中途截断（curl 退出码 56）；
- qtextended.org 官方源已下线（404）。

## 并行 Range 分块下载

SourceForge 支持 HTTP Range，可用多连接并行下载（8 连接实测约 200 KB/s，约 15 分钟）：

```bash
URL="https://downloads.sourceforge.net/project/qpe/QPE/qtopia/qt-extended-opensource-src-4.4.3.tar.gz"
TOTAL=190054400
N=8
CS=$(( (TOTAL + N - 1) / N ))

fetch() {
  local i=$1 s=$2 e=$3 out=".part$i" expect=$(( $3-$2+1 )) fails=0
  while :; do
    cur=$(stat -c%s "$out" 2>/dev/null || echo 0)
    [ "$cur" -ge "$expect" ] && break
    rs=$((s+cur))
    if curl -fsSL --connect-timeout 20 --max-time 240 -r ${rs}-${e} "$URL" >> "$out"; then
      fails=0
    else
      fails=$((fails+1))
      [ "$fails" -ge 60 ] && return 1
    fi
  done
}

for i in $(seq 1 $N); do
  s=$(( (i-1)*CS )); e=$(( s+CS-1 )); [ $e -ge $TOTAL ] && e=$((TOTAL-1))
  fetch $i $s $e &
done
wait

cat .part1 .part2 .part3 .part4 .part5 .part6 .part7 .part8 > qt-extended-opensource-src-4.4.3.tar.gz
sha256sum qt-extended-opensource-src-4.4.3.tar.gz   # 对照上面的期望值
gzip -t qt-extended-opensource-src-4.4.3.tar.gz && echo OK
tar -xzf qt-extended-opensource-src-4.4.3.tar.gz
```

## 关键坑：curl 内部重试会重复追加

`curl --retry` 与 `>>` 重定向组合时，重试会把同一 Range 再次追加到文件，导致分块大小翻倍（实测某块变成 46 MB，应为 23 MB）。因此：

- **不要在分块下载时用 `curl --retry`**；
- 重试放在 shell 层（上面的 `while` 循环），每次按当前文件字节数重算 Range 起点（`rs=s+cur`）；
- 拼接后务必 `sha256sum` + `gzip -t` 双重校验；
- 分块大小全部正确后再 cat，否则会出现 gzip 流损坏。
