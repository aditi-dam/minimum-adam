set shell := ["bash", "-eu", "-o", "pipefail", "-c"]

root := justfile_directory()
minemu := env_var_or_default("MINEMU", "minemu")

# Build the starter and supplied examples.
build:
    make -C "{{ root }}"

# Package the current kernel and user modules.
image:
    make -C "{{ root }}" MINEMU="{{ minemu }}" image

# Run one manifest, for example: just test hw1 echo
test hw name: image
    if [[ -f "{{ root }}/tests/{{ hw }}/Makefile" ]]; then make -C "{{ root }}/tests/{{ hw }}" MINEMU="{{ minemu }}" "prepare-{{ name }}"; fi
    {{ minemu }} test "{{ root }}/tests/{{ hw }}/{{ name }}.toml"

# Run every manifest for one homework, for example: just test-all hw1
test-all hw: image
    #!/usr/bin/env bash
    if [[ -f "{{ root }}/tests/{{ hw }}/Makefile" ]]; then
      make -C "{{ root }}/tests/{{ hw }}" MINEMU="{{ minemu }}" all
    fi
    shopt -s nullglob
    manifests=("{{ root }}"/tests/{{ hw }}/*.toml)
    if (( ${#manifests} == 0 )); then
      printf 'no tests found for %s\n' "{{ hw }}" >&2
      exit 2
    fi
    status=0
    for manifest in "${manifests[@]}"; do
      printf '==> %s\n' "${manifest##*/}"
      {{ minemu }} test "$manifest" || status=$?
    done
    exit $status

# Remove generated guest build output.
clean:
    make -C "{{ root }}" clean

# start the docker container
docker:
    docker run --rm -it \
        --add-host host.docker.internal:host-gateway \
        -v "$PWD:/workspace" \
        -w /workspace \
        rtsang1/cs492-stevens:latest
