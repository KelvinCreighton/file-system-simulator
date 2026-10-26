# File System Simulator

A small C program that simulates a block based file system using disk image files. It supports directories, file allocation, reading and writing blocks, resizing files, and defragmenting a disk. Commands are read from a text file and applied in order.

## Build

Requirements: GCC and Make.

```sh
make
```

This builds the simulator as `./fs`. To remove the executable and object file:

```sh
make clean
```

## Create a disk image

The repository includes the `create_fs` disk initializer executable. Run it with the image filename:

```sh
./create_fs disk.img
```

The simulator expects a 128-block disk image. Each block is 1 KiB; block 0 contains the superblock, leaving 127 data blocks. The superblock tracks free blocks and up to 126 inodes.

## Run commands

Pass a command file to the simulator:

```sh
./fs commands.txt
```

Commands are one per line. Mount a disk before issuing filesystem operations. File and directory names are at most five characters; name comparisons are case-insensitive.

| Command | Syntax | Description |
| --- | --- | --- |
| `M` | `M disk.img` | Mount a disk image. |
| `C` | `C name size` | Create a file of `size` blocks, or a directory when `size` is `0`. |
| `D` | `D name` | Delete a file or directory (and its contents). |
| `R` | `R name block` | Read a file block into the current buffer and print it. |
| `W` | `W name block` | Write the current buffer to a file block. |
| `B` | `B text...` | Set the buffer to the provided text; spaces between words are preserved. |
| `L` | `L` | List the current directory. |
| `E` | `E name size` | Resize a file to `size` blocks. |
| `O` | `O` | Defragment the mounted disk. |
| `Y` | `Y name` | Change directory; `.` and `..` navigate relative to the current directory. |

For example, save the following as `commands.txt` in the repository root:

```text
M disk.img
C notes 1
B Hello from the simulator
W notes 0
R notes 0
L
```

Then run `./fs commands.txt`. Mutating commands update the disk image.

## Fixtures

`tests/test1` through `tests/test4` contain command inputs, disk images, and expected standard output, standard error, and disk image results. Run an individual fixture with, for example:

```sh
./fs tests/test1/input
```

The fixtures are reference data; the repository does not include a test runner.

