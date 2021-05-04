import pytest

from fixtures import empty_paths as paths, executables


def test_missing_paths(paths, executables):
    for executable in executables:
        with pytest.raises(Exception):
            paths.get_path_of(executable.name)
