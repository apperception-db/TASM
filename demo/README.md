# Build TASM for the GPU.
`docker build -t tasm/environment-gpu -f docker/Dockerfile.environment  .`
`docker build -t tasm/tasm -f docker/Dockerfile .`

# Run the TASM docker.
Replace `$(pwd)/data` with whatever path that contains the labels database/resources catalog. When the `tasm` object is created, update its configuration accordingly.
`docker run --rm -it -p 8890:8890 --gpus=all -v $(pwd)/data:/data --name tasm tasm/tasm:latest /bin/bash`

# Try the demo.
In the docker:
`jupyter notebook --ip 0.0.0.0 --port 8890 --allow-root &`

On the local machine:
`ssh -L 8890:127.0.0.1:8890 <user>@<host>`

The demo notebook first adds the detections to the database. It associates each bounding box both with its object type and object ID so that either can be queried for. It then stores the original video both without tiles and with tiles. The tiled version skips tiling GOPs where tiling likely wouldn't help performance. It then tests creating cropped videos of a specific object.