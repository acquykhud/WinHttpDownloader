## Usage 
```
Usage: WinHttpDownloader.exe [options...]
Options:
    -t, --thread           number of thread (default 1)
    -c, --conn             number of connection (default 1)
    -u, --url              URL                     (Required)
    -o, --out              Path to save file       (Required)
    -h, --help             Shows this page
```

## Example
```
WinHttpDownloader.exe -c 9 -t 3 -o C:\out.bin -u https://speed.hetzner.de/100MB.bin
```