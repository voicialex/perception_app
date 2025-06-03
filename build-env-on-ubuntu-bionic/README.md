# docker-cross-compiler-novnc

Docker-based build environment based on [dorowu/ubuntu-desktop-lxde-vnc](https://hub.docker.com/r/dorowu/ubuntu-desktop-lxde-vnc) with pre-built and configured toolchains for cross compiling.

---
## Usage
### Installation
```
# docker load < /shared/000_downloads/docker-ubuntu-lxde-novnc.tar
Loaded image ID: sha256:60c9ff2845245aec33e47a6e4697dbc054fc4d65c48ac8a1e451d9259ebf49d8
# docker tag 60c9ff2845245aec33e47a6e4697dbc054fc4d65c48ac8a1e451d9259ebf49d8 docker-ubuntu-lxde-novnc:latest
```
checking:
```
# docker images
REPOSITORY                 TAG             IMAGE ID       CREATED        SIZE
docker-ubuntu-lxde-novnc   latest          60c9ff284524   4 hours ago    4.92GB
```

### Run
*Note: Mapping the /dev directory is to make the newly created loop devices appear in the container.*
```
mkdir ~/work
chown 1000:1000 ~/work
docker run --rm --privileged -v /dev:/dev \
    --name docker-ubuntu-lxde-novnc \
    -p 6080:80 \
    -p 5900:5900 \
    -e HTTP_PASSWORD=password \
    -e VNC_PASSWORD=password \
    -e PUID=1000 \
    -e PGID=1000 \
    -e USER=ubuntu \
    -e PASSWORD=ubuntu \
    -v ~/.gitconfig:/home/ubuntu/.gitconfig:ro \
    -v ~/work:/home/ubuntu/work \
    -e RESOLUTION=1280x720 \
    docker-ubuntu-lxde-novnc:latest
```
### Check your git configuration
```
docker exec -it --user ubuntu docker-ubuntu-lxde-novnc bash -c 'git config --list'
```
### Get a bash shell
```
docker exec -it --user ubuntu --workdir /home/ubuntu docker-ubuntu-lxde-novnc bash
```
### VNC Viewer
open the vnc viewer and connect to port 5900.
### Web brower
Browse http://127.0.0.1:6080/
## Test Cases
Successfully compiled the following projects:
- [x] android 8.1
- [x] android 10
- [x] android 12 (tv & tablet)
- [x] kernel 4.4
- [x] kernel 4.19
- [x] kernel 5.10
- [x] kernel 5.15
- [x] uboot v2014.10
- [x] uboot v2017.09
- [x] friendlywrt v22.03
- [x] friendlywrt v21.02
- [x] buildroot
- [x] package the image by using sd-fuse_xxx
---
## Environment Variables

### `FASTBOOT`
* `true`  
Faster container initialization by skipping `chown`-ing every files and directories under `$HOME` on container startup. This may be useful when volume is linked on `$HOME` or its subdirectory, and contains lots of files and directories. __Enabling this option might cause files under `$HOME` inaccessible by container.__
* `false`  
`chown` every file under `$HOME` on container startup.
* **DEFAULT** `false`

### `RESOLUTION`
* Set screen resolution in `NNNNxNNNN` form, like `1366x768`.  
* **DEFAULT** _Follows the size of the browser window when first connected._  

### `USERNAME`
* Name of default user.  
* **DEFAULT** `root`

### `PASSWORD`
* Password of the user inside the container. This may required if you want to use SSH with password authentication, or normal user rather than `root`.

### `HTTP_PASSWORD`
* Password for authentication before loading noVNC screen. `USERNAME` is used as username. Password may be sent without any protection - use other authentication method when possible if this container is planned to be run as worldwide-public.

### `VNC_PASSWORD`
* Authentication method provided by noVNC. Password longer than 8 characters will be truncated to 8 characters.

---
## Acknowledgments
- [fcwu/docker-ubuntu-vnc-desktop](https://github.com/fcwu/docker-ubuntu-vnc-desktop)
- [hdavid0510/docker-ubuntu-lxde-novnc](https://github.com/hdavid0510/docker-ubuntu-lxde-novnc)
- [dorowu/ubuntu-desktop-lxde-vnc](https://hub.docker.com/r/dorowu/ubuntu-desktop-lxde-vnc)
