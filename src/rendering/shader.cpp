#include "shader.hpp"

#include "core/log.hpp"
#include "misc/utils.hpp"
#include "texture.hpp"

#include <sstream>

Shader::Shader(const char *vertSource, const char *fragSource): _id(0)
{
    unsigned int vertShader, fragShader;
    
    // Create and compile VERTEX shader
    vertShader = GL_CALL(glad_glCreateShader(GL_VERTEX_SHADER));
    GL_CALL(glad_glShaderSource(vertShader, 1, &vertSource, 0));
    GL_CALL(glad_glCompileShader(vertShader));
    CheckShaderForErrors(vertShader);

    // Create and compile FRAGMENT shader
    fragShader = GL_CALL(glad_glCreateShader(GL_FRAGMENT_SHADER));
    GL_CALL(glad_glShaderSource(fragShader, 1, &fragSource, 0));
    GL_CALL(glad_glCompileShader(fragShader));
    CheckShaderForErrors(fragShader);

    // Link PROGRAM
    _id = GL_CALL(glad_glCreateProgram());
    GL_CALL(glad_glAttachShader(_id, vertShader));
    GL_CALL(glad_glAttachShader(_id, fragShader));
    GL_CALL(glad_glLinkProgram(_id));

    GL_CALL(glad_glDeleteShader(vertShader));
    GL_CALL(glad_glDeleteShader(fragShader));

    // Uniform parsing
    std::stringstream vertStream(vertSource), fragStream(fragSource);
    std::string line;
    // Go through every line of the VERTEX shader
    // and save the shader uniform if one was declared on the given line
    while(vertStream.good())
    {
        std::getline(vertStream, line, '\n');
        
        auto uniform = ParseShaderUniformLine(line);
        if(uniform != nullptr)
            _uniforms.push_back(std::move(uniform));    
    }
    line.clear();

    // Go through every line of the FRAGMENT shader
    // and save the shader uniform if one was declared on the given line
    while(fragStream.good())
    {
        std::getline(fragStream, line, '\n');
        
        auto uniform = ParseShaderUniformLine(line);
        if(uniform != nullptr)
            _uniforms.push_back(std::move(uniform));    
    }
}
Shader::~Shader()
{
    GL_CALL(glad_glDeleteProgram(_id));
}
// Copy
Shader::Shader(const Shader& other)
{
    if(&other != this)
    {
        this->_id = other._id;
        this->_uniforms = other._uniforms;
    }
}
Shader& Shader::operator=(Shader other)
{
    if(&other != this)
    {
        this->_id = other._id;
        this->_uniforms = other._uniforms;
    }
    return *this;
}
// Move
Shader::Shader(Shader&& other)
{
    if(&other != this)
    {
        this->_id = std::move(other._id);
        this->_uniforms = std::move(other._uniforms);
    }
}
Shader& Shader::operator=(Shader&& other)
{
    if(&other != this)
    {
        this->_id = std::move(other._id);
        this->_uniforms = std::move(other._uniforms);
    }
    return *this;
}

void Shader::Bind() const
{
    GL_CALL(glad_glUseProgram(_id));
    UpdateUniforms();
}
void Shader::Unbind() const
{
    GL_CALL(glad_glUseProgram(0));
}

void Shader::SetUniform(const std::string &name, const void* value)
{
    for(auto uniform: _uniforms)
    {
        if(uniform->getName() == name)
        {
            uniform->value = const_cast<void*>(value);
        }
    }
}
void Shader::UpdateUniforms() const
{
    // Go through each uniform and update its value
    // The appropriate function must be used for the appropriate type 
    for(auto &uniform: _uniforms)
    {
        unsigned int uniformLocation = GL_CALL(glad_glGetUniformLocation(_id, uniform->getName().c_str()));
        switch(uniform->getType())
        {
            case ShaderUniformType::INT:
            case ShaderUniformType::BOOL:
                GL_CALL(glad_glUniform1i(uniformLocation, *((int*)(uniform->value))));
            break;
            case ShaderUniformType::UINT:
                GL_CALL(glad_glUniform1ui(uniformLocation, *((unsigned int*)(uniform->value))));
            break;
            case ShaderUniformType::FLOAT:
                GL_CALL(glad_glUniform1f(uniformLocation, *((float*)(uniform->value))));
            break;


            case ShaderUniformType::VEC2:
                GL_CALL(glad_glUniform2fv(uniformLocation, 1, (float*)(uniform->value)));
            break;
            case ShaderUniformType::VEC3:
                GL_CALL(glad_glUniform3fv(uniformLocation, 1, (float*)(uniform->value)));
            break;
            case ShaderUniformType::VEC4:
                GL_CALL(glad_glUniform4fv(uniformLocation, 1, (float*)(uniform->value)));
            break;


            case ShaderUniformType::MAT2:
                GL_CALL(glad_glUniformMatrix2fv(uniformLocation, 1, false, (float*)(uniform->value)));
            break;
            case ShaderUniformType::MAT3:
                GL_CALL(glad_glUniformMatrix3fv(uniformLocation, 1, false, (float*)(uniform->value)));
            break;
            case ShaderUniformType::MAT4:
                GL_CALL(glad_glUniformMatrix4fv(uniformLocation, 1, false, (float*)(uniform->value)));
            break;


            case ShaderUniformType::TEX2D:
                GL_CALL(glad_glUniform1i(uniformLocation, ((Texture*)(uniform->value))->getTextureImageUnit()));
            break;
        }
    }
}

// Reports on the potential shader compile errors
// so that they are known and can be fixed
void Shader::CheckShaderForErrors(unsigned int shader)
{
    int success;
    char infoLog[512];

    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if(!success)
    {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        Log::LogError("Shader error: " + std::string(infoLog));
    }
}

// Parses the specified line of shader code and checks if a uniform is declared on it
ShaderUniform* const Shader::ParseShaderUniformLine(const std::string &line)
{
    if(line.empty() || line[0] == '#')
        return nullptr;

    /* 
    Separate the line into tokens
    Format: (layout) uniform type name = default_value
    
    NOTE: It'd be a good idea to add support for layout so that
    texture binding points can be specified in the shader
    and so that the uniform doesn't get ignored because the first token
    on the line is something other than "uniform"
    */
    auto splitLine = SplitString(line, ' ');

    if(splitLine[0].compare("uniform") == 0)
    {
        std::string name;
        ShaderUniformType type;
        void *value = nullptr;
        ShaderUniform* uniform;

        name = splitLine[2];
        // It's important to delete the ; because otherwise,
        // glUniformLocation would return the wrong value.
        // OpenGL doesn't store the uniform names with a ; internally
        if(name.at(name.size() - 1) == ';')
            name.erase(name.end() - 1);

        // Set the uniform type appropriately
        // Sadly, C++ switch statements don't support strings so if-elseif chain it is
        if(splitLine[1].compare("int") == 0)
            type = ShaderUniformType::INT;
        else if(splitLine[1].compare("uint") == 0)
            type = ShaderUniformType::UINT;
        else if(splitLine[1].compare("float") == 0)
            type = ShaderUniformType::FLOAT;
        else if(splitLine[1].compare("bool") == 0)
            type = ShaderUniformType::BOOL;
        else if(splitLine[1].compare("vec2") == 0)
            type = ShaderUniformType::VEC2;
        else if(splitLine[1].compare("vec3") == 0)
            type = ShaderUniformType::VEC3;
        else if(splitLine[1].compare("vec4") == 0)
            type = ShaderUniformType::VEC4;
        else if(splitLine[1].compare("mat2") == 0)
            type = ShaderUniformType::MAT2;
        else if(splitLine[1].compare("mat3") == 0)
            type = ShaderUniformType::MAT3;
        else if(splitLine[1].compare("mat4") == 0)
            type = ShaderUniformType::MAT4;
        else if(splitLine[1].compare("sampler2D") == 0)
            type = ShaderUniformType::TEX2D;


        unsigned int uniformIndex = 0;
        // The C-string version of the name must be saved
        // in a variable before using it because of some C++ shenanigans
        // (otherwise the name doesn't get passed along at all or is messed up)
        const char* nameCStr = name.c_str();
        GL_CALL(glad_glGetUniformIndices(_id, 1, &nameCStr, &uniformIndex));

        // Create the value ptr and set it to a value of the appropriate type
        switch((ShaderUniformType)type)
        {
            case ShaderUniformType::INT:
            case ShaderUniformType::BOOL:
                value = (void*)new int;
                GL_CALL(glad_glGetUniformiv(_id, uniformIndex, (int*)value));
            break;
            case ShaderUniformType::UINT:
                value = (void*)new unsigned int;
                GL_CALL(glad_glGetUniformuiv(_id, uniformIndex, (unsigned int*)value));
            break;
            case ShaderUniformType::FLOAT:
                value = (void*)new float;
                GL_CALL(glad_glGetUniformfv(_id, uniformIndex, (float*)value));
            break;
            

            case ShaderUniformType::VEC2:
                value = (void*)new float[2];
                GL_CALL(glad_glGetUniformfv(_id, uniformIndex, (float*)value));      
            break;
            case ShaderUniformType::VEC3:
                value = (void*)new float[3];
                GL_CALL(glad_glGetUniformfv(_id, uniformIndex, (float*)value));     
            break;
            case ShaderUniformType::VEC4:
                value = (void*)new float[4];
                GL_CALL(glad_glGetUniformfv(_id, uniformIndex, (float*)value));   
            break;
            

            case ShaderUniformType::MAT2:  
                value = (void*)new float[2*2];
                GL_CALL(glad_glGetUniformfv(_id, uniformIndex, (float*)value));
            break;
            case ShaderUniformType::MAT3:  
                value = (void*)new float[3*3];
                GL_CALL(glad_glGetUniformfv(_id, uniformIndex, (float*)value));   
            break;
            case ShaderUniformType::MAT4:  
                value = (void*)new float[4*4];
                GL_CALL(glad_glGetUniformfv(_id, uniformIndex, (float*)value)); 
            break;


            case ShaderUniformType::TEX2D:
                value = (void*)new Texture;
            break;
        }

        uniform = new ShaderUniform(name, (ShaderUniformType)type, (void*)value);
        return uniform;
    }
    else
        return nullptr;
}