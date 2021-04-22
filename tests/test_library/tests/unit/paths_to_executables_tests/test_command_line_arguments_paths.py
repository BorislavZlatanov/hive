from fixtures import empty_paths as paths, executables


def test_command_line_arguments_paths(paths, executables):
    for executable in executables:
        paths.parse_command_line_arguments([executable.argument, executable.path])
        assert paths.get_path_of(executable.name) == executable.path
