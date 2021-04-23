class NotSupported(Exception):
    pass


class _PathsToExecutables:
    class __ExecutableDetails:
        def __init__(self, name, default_path_from_build):
            self.name = name
            self.argument = f'--{name.replace("_", "-")}-path'
            self.environment_variable = f'{name}_PATH'.upper()
            self.default_path_from_build = default_path_from_build

    def __init__(self):
        self.paths = {}
        self.command_line_arguments = None
        self.environment_variables = None
        self.installed_executables = None
        self.supported_executables = [
            self.__ExecutableDetails('hived', 'programs/hived/hived'),
            self.__ExecutableDetails('cli_wallet', 'programs/cli_wallet/cli_wallet'),
            self.__ExecutableDetails('get_dev_key', 'programs/util/get_dev_key'),
        ]

        self.parse_command_line_arguments()
        self.set_environment_variables()
        self.set_installed_executables()

    def __is_supported(self, executable_name):
        return any([executable_name == executable.name for executable in self.supported_executables])

    def print_configuration_hint(self):
        hive_build_path = 'HIVE_BUILD_PATH'
        print(f'Edit {hive_build_path} below, add following lines to /etc/environment and restart computer.\n')

        print(f'{hive_build_path}= # Should be something like: \'/home/dev/hive/build\'')
        for executable in self.supported_executables:
            print(f'{executable.environment_variable}=\'${{{hive_build_path}}}/{executable.default_path_from_build}\'')

    def print_paths_in_use(self):
        entries = []
        for executable in self.supported_executables:
            entries += [
                f'Name: {executable.name}\n'
                f'Path: {self.get_path_of(executable.name)}\n'
            ]
        print('\n'.join(entries))

    def get_path_of(self, executable_name):
        if not self.__is_supported(executable_name):
            raise NotSupported(f'Executable {executable_name} is not supported')

        if executable_name in self.paths:
            return self.paths[executable_name]

        if getattr(self.command_line_arguments, executable_name) is not None:
            return getattr(self.command_line_arguments, executable_name)

        if executable_name in self.environment_variables and self.environment_variables[executable_name] is not None:
            return self.environment_variables[executable_name]

        if executable_name in self.installed_executables and self.installed_executables[executable_name] is not None:
            return self.installed_executables[executable_name]

        self.print_configuration_hint()
        raise Exception(f'Missing path to {executable_name}')

    def set_path_of(self, executable_name, executable_path):
        if not self.__is_supported(executable_name):
            raise NotSupported(f'Executable {executable_name} is not supported')

        self.paths[executable_name] = executable_path

    def parse_command_line_arguments(self, arguments=None):
        from argparse import ArgumentParser
        parser = ArgumentParser()
        for executable in self.supported_executables:
            parser.add_argument(executable.argument, dest=executable.name)

        self.command_line_arguments, _ = parser.parse_known_args(arguments)

    def set_environment_variables(self, variables=None):
        self.environment_variables = {}

        if variables is None:
            from os import path, getenv
            for executable in self.supported_executables:
                environment_variable = getenv(executable.environment_variable)
                if environment_variable is not None:
                    environment_variable = path.expandvars(environment_variable)
                self.environment_variables[executable.name] = environment_variable
            return

        for executable in self.supported_executables:
            if executable.environment_variable in variables.keys():
                self.environment_variables[executable.name] = variables[executable.environment_variable]
            else:
                self.environment_variables[executable.name] = None

    def set_installed_executables(self, installed_executables=None):
        self.installed_executables = {}

        if installed_executables is None:
            import shutil
            for executable in self.supported_executables:
                self.installed_executables[executable.name] = shutil.which(executable.name)
            return

        for executable in self.supported_executables:
            if executable.name in installed_executables.keys():
                self.installed_executables[executable.name] = installed_executables[executable.name]
            else:
                self.installed_executables[executable.name] = None


__paths = _PathsToExecutables()


def get_path_of(executable_name):
    return __paths.get_path_of(executable_name)


def set_path_of(executable_name, executable_path):
    __paths.set_path_of(executable_name, executable_path)


def print_paths_in_use():
    __paths.print_paths_in_use()
