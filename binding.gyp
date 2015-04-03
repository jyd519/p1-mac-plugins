{
    'targets': [
        {
            'target_name': 'native',
            'dependencies': [
                'deps/syphon/syphon.gyp:Syphon'
            ],
            'include_dirs': [
                '$(p1stream_include_dir)'
            ],
            'sources': [
                'src/display_link.cc',
                'src/display_stream.cc',
                'src/detect_displays.cc',
                'src/audio_queue.cc',
                'src/detect_audio_inputs.cc',
                'src/syphon_client.mm',
                'src/syphon_directory.mm',
                'src/preview_service.cc',
                'src/module.mm'
            ],
            'xcode_settings': {
                # 'MACOSX_DEPLOYMENT_TARGET': '10.8',  # FIXME: This doesn't work
                'CLANG_ENABLE_OBJC_ARC': 'YES',
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
