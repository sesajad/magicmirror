#Building

```bash
docker build -t magicmirror .
```

# Running

```bash
docker run --rm --net host -t magicmirror 3000 127.0.0.1 3001 56000-56001 82.165.204.172 55000 192.168.0.15/16 64500-65000
```

This will result in the following configuration:

```
Near listen port: 3000
Near endpoint: 127.0.0.1:3001
Far listen port range: 56000 - 56001
Far endpoint: 82.165.204.172:55000
Spoofed IP range: 192.168.0.0 - 192.168.255.255
Spoofed port range: 64500 - 65000
```
