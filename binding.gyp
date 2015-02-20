{
    'targets': [
        {
            'target_name': 'native',
            'include_dirs': [
                '$(p1stream_include_dir)'
            ],
            'sources': [
                'src/display_link.cc',
                'src/display_stream.cc',
                'src/detect_displays.cc',
                'src/audio_queue.cc',
                'src/preview_service.cc',
                'src/module.cc'
            ],
            'xcode_settings': {
                # 'MACOSX_DEPLOYMENT_TARGET': '10.8',  # FIXME: This doesn't work
                'OTHER_CFLAGS': ['-mmacosx-version-min=10.8', '-std=c++11', '-stdlib=libc++']
            },
            'link_settings': {
                'libraries': [
                    '$(SDKROOT)/System/Library/Frameworks/CoreGraphics.framework',
                    '$(SDKROOT)/System/Library/Frameworks/OpenCL.framework'
                ]
            }
        }
    ]
}
