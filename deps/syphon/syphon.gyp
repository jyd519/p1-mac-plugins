{
    'variables': {
        'syphon_include_dirs': [
            'Syphon-Framework'
        ]
    },
    'target_defaults': {
        'include_dirs': ['Syphon-Framework'],
        'all_dependent_settings': {
            'include_dirs': ['generated'],
            'link_settings': {
                'libraries': [
                    '$(SDKROOT)/System/Library/Frameworks/Cocoa.framework'
                ]
            }
        },
        'xcode_settings': {
            'GCC_PRECOMPILE_PREFIX_HEADER': 'YES',
            'GCC_PREFIX_HEADER': 'Syphon-Framework/Syphon_Prefix.pch',
            # 'MACOSX_DEPLOYMENT_TARGET': '10.8',  # FIXME: This doesn't work
            'OTHER_CFLAGS': ['-mmacosx-version-min=10.8', '-std=gnu99'],
            'WARNING_CFLAGS': ['-w']
        }
    },
    'targets': [
        {
            'target_name': 'Syphon',
            'type': 'static_library',
            'sources': [
                'Syphon-Framework/SyphonServer.m',
                'Syphon-Framework/SyphonClient.m',
                'Syphon-Framework/SyphonClientConnectionManager.m',
                'Syphon-Framework/SyphonServerDirectory.m',
                'Syphon-Framework/SyphonPrivate.m',
                'Syphon-Framework/SyphonServerConnectionManager.m',
                'Syphon-Framework/SyphonMessageReceiver.m',
                'Syphon-Framework/SyphonCFMessageReceiver.m',
                'Syphon-Framework/SyphonMessageSender.m',
                'Syphon-Framework/SyphonCFMessageSender.m',
                'Syphon-Framework/SyphonMessaging.m',
                'Syphon-Framework/SyphonIOSurfaceImage.m',
                'Syphon-Framework/SyphonImage.m',
                'Syphon-Framework/SyphonMessageQueue.m',
                'Syphon-Framework/SyphonDispatch.c',
                'Syphon-Framework/SyphonOpenGLFunctions.c'
            ]
        }
    ]
}
