install_subdir('config_files_private',
    strip_directory: true,
    install_dir: get_option('datadir')/'phosphor-regulators')

subdir('src')

if get_option('tests').enabled()
    subdir('test')
endif
