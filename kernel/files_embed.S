# kernel/files_embed.S — Embed external files using .incbin
# Files from Windows host are embedded as binary data with named symbols.

.section .rodata

.globl _binary_files_hello_txt_start
.globl _binary_files_hello_txt_end
.globl _binary_files_hello_txt_size

_binary_files_hello_txt_start:
    .incbin "disk/hello.txt"
_binary_files_hello_txt_end:
    .set _binary_files_hello_txt_size, _binary_files_hello_txt_end - _binary_files_hello_txt_start

.globl _binary_files_readme_txt_start
.globl _binary_files_readme_txt_end
.globl _binary_files_readme_txt_size

_binary_files_readme_txt_start:
    .incbin "disk/readme.txt"
_binary_files_readme_txt_end:
    .set _binary_files_readme_txt_size, _binary_files_readme_txt_end - _binary_files_readme_txt_start

.globl _binary_test_txt_start
.globl _binary_test_txt_end
.globl _binary_test_txt_size

_binary_test_txt_start:
    .incbin "disk/test.txt"
_binary_test_txt_end:
    .set _binary_test_txt_size, _binary_test_txt_end - _binary_test_txt_start
