# utils.cmake-build-debug

#重新定义当前目标的源文件的__FILE__宏
function(redefine_file_macro targetname)
    #获取当前目标的所有源文件
    get_target_property(source_files "${targetname}" SOURCES)
    #遍历源文件
    foreach(sourcefile ${source_files})
        #获取当前源文件的编译参数
        get_property(defs SOURCE "${sourcefile}"
                PROPERTY COMPILE_DEFINITIONS)
        #获取当前文件的绝对路径
        get_filename_component(filepath "${sourcefile}" ABSOLUTE)
        #将绝对路径中的项目路径替换成空,得到源文件相对于项目路径的相对路径
        string(REPLACE ${PROJECT_SOURCE_DIR}/ "" relpath ${filepath})
        #将我们要加的编译参数(__FILE__定义)添加到原来的编译参数里面
        list(APPEND defs "__FILE__=\"${relpath}\"")
        #重新设置源文件的编译参数
        set_property(
                SOURCE "${sourcefile}"
                PROPERTY COMPILE_DEFINITIONS ${defs}
        )
    endforeach()
endfunction()

function(force_redefine_file_macro_for_sources targetname)
    get_target_property(source_files "${targetname}" SOURCES)
    foreach(sourcefile ${source_files})
        # Get source file's current list of compile definitions.
        get_property(defs SOURCE "${sourcefile}"
                PROPERTY COMPILE_DEFINITIONS)
        # Get the relative path of the source file in project directory
        get_filename_component(filepath "${sourcefile}" ABSOLUTE)
        string(REPLACE ${PROJECT_SOURCE_DIR}/ "" relpath ${filepath})
        list(APPEND defs "__FILE__=\"${relpath}\"")
        # Set the updated compile definitions on the source file.
        set_property(
                SOURCE "${sourcefile}"
                PROPERTY COMPILE_DEFINITIONS ${defs}
        )
    endforeach()
endfunction()

function(acid_add_executable targetname srcs depends libs)
    add_executable(${targetname} ${srcs})
    add_dependencies(${targetname} ${depends})
    force_redefine_file_macro_for_sources(${targetname})
    target_link_libraries(${targetname} ${libs})
endfunction()