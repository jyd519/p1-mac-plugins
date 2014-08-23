{
    'targets': [
        {
            'target_name': 'native',
            'include_dirs': [
                '$(p1stream_include_dir)'
            ],
            'sources': [
                'src/audio_queue.cc',
                'src/display_link.cc',
                'src/display_stream.cc',
                'src/module.cc',
            ],
            'xcode_settings': {
                # 'MACOSX_DEPLOYMENT_TARGET': '10.8',  # FIXME: This doesn't work
                'OTHER_CFLAGS': ['-mmacosx-version-min=10.8', '-std=c++11', '-stdlib=libc++']
            }
        }
    ]
}
