#pragma once
/*
MIT License

Copyright (c) 2021 abhilashraju

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <glad/glad.h> 
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>
#include "assimp/Importer.hpp"
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "mesh.hpp"
#include "gl.hpp"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>

namespace gl {
    unsigned int TextureFromFile(const char* path, const std::string& directory, bool gamma = false);

    class Model
    {
    public:
        // model data 
        std::vector<Texture> textures_loaded;	// stores all the textures loaded so far, optimization to make sure textures aren't loaded more than once.
        std::vector<Mesh>    meshes;
        std::string directory;
        bool gammaCorrection;
        GProgramme debugShader_;
        // constructor, expects a filepath to a 3D model.
        Model(std::string const& path, bool gamma = false) : gammaCorrection(gamma)
        {
            auto on_failure = [](const std::error_code& code,auto log) {
                std::cout << code.message() << log << std::endl;
                
            };
            debugShader_= make_programme(make_shader(R"(#version 330 core
                                    layout (location = 0) in vec3 aPos;
                                    layout (location = 1) in vec3 aNormal;

                                    out VS_OUT {
                                        vec3 normal;
                                    } vs_out;

                                    uniform mat4 view;
                                    uniform mat4 model;

                                    void main()
                                    {
                                        mat3 normalMatrix = mat3(transpose(inverse(view * model)));
                                        vs_out.normal = normalMatrix * aNormal;
                                        gl_Position = view * model * vec4(aPos, 1.0); 
                                    })",GL_VERTEX_SHADER),
                                    make_shader(R"(#version 330 core
                                    out vec4 FragColor;

                                    void main()
                                    {
                                        FragColor = vec4(0.0, 1.0, 0.0, 1.0);
                                    }
                                    )",GL_FRAGMENT_SHADER),
                                    make_shader(R"(#version 330 core
                                    layout (triangles) in;
                                    layout (line_strip, max_vertices = 6) out;

                                    in VS_OUT {
                                        vec3 normal;
                                    } gs_in[];

                                    const float MAGNITUDE = 0.2;

                                    uniform mat4 projection;

                                    void GenerateLine(vec4 Pos,vec3 N)
                                    {
                                        gl_Position = projection * Pos;
                                        EmitVertex();
                                        gl_Position = projection * (Pos + vec4(N, 0.0) * MAGNITUDE);
                                        EmitVertex();
                                        EndPrimitive();
                                    }
                                    void main()
                                    {
                                        GenerateLine(gl_in[0].gl_Position,vec3(normalize(gs_in[0].normal))); // first vertex normal
                                        GenerateLine(gl_in[1].gl_Position,vec3(normalize(gs_in[1].normal))); // second vertex normal
                                        GenerateLine(gl_in[2].gl_Position,vec3(normalize(gs_in[2].normal))); // third vertex normal  
                                       
                                    })",GL_GEOMETRY_SHADER))(on_failure);
            loadModel(path);
        }

        // draws the model, and thus all its meshes
        GProgramme& debugShader(){return debugShader_;}
        void drawNormals(const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection){
            
            draw(debugShader_,model,view,projection);
        }
        void draw(GProgramme& shader)
        {
            for (unsigned int i = 0; i < meshes.size(); i++)
                meshes[i].Draw(shader);
        }
        void draw(GProgramme& shader, const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection)
        {
            shader.use();
            shader.setUniformMatrix4("view", 1, GL_FALSE, glm::value_ptr(view));
            shader.setUniformMatrix4("projection", 1, GL_FALSE, glm::value_ptr(projection));
            shader.setUniformMatrix4("model", 1, GL_FALSE, glm::value_ptr(model));
            for (unsigned int i = 0; i < meshes.size(); i++)
                meshes[i].Draw(shader);
        }

    private:
        // loads a model with supported ASSIMP extensions from file and stores the resulting meshes in the meshes vector.
        void loadModel(std::string const& path)
        {
            // read file via ASSIMP
            Assimp::Importer importer;
            const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
            // check for errors
            if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
            {
                auto error = importer.GetErrorString();
                std::cout << "ERROR::ASSIMP:: " << error << std::endl;
                return;
            }
            // retrieve the directory path of the filepath
            directory = path.substr(0, path.find_last_of('/'));

            // process ASSIMP's root node recursively
            processNode(scene->mRootNode, scene);
        }

        // processes a node in a recursive fashion. Processes each individual mesh located at the node and repeats this process on its children nodes (if any).
        void processNode(aiNode* node, const aiScene* scene)
        {
            // process each mesh located at the current node
            for (unsigned int i = 0; i < node->mNumMeshes; i++)
            {
                // the node object only contains indices to index the actual objects in the scene. 
                // the scene contains all the data, node is just to keep stuff organized (like relations between nodes).
                aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
                meshes.push_back(std::move(processMesh(mesh, scene)));
            }
            // after we've processed all of the meshes (if any) we then recursively process each of the children nodes
            for (unsigned int i = 0; i < node->mNumChildren; i++)
            {
                processNode(node->mChildren[i], scene);
            }

        }

        Mesh processMesh(aiMesh* mesh, const aiScene* scene)
        {
            // data to fill
            std::vector<Vertex> vertices;
            std::vector<unsigned int> indices;
            std::vector<Texture> textures;

            // walk through each of the mesh's vertices
            for (unsigned int i = 0; i < mesh->mNumVertices; i++)
            {
                Vertex vertex;
                glm::vec3 vector; // we declare a placeholder vector since assimp uses its own vector class that doesn't directly convert to glm's vec3 class so we transfer the data to this placeholder glm::vec3 first.
                // positions
                vector.x = mesh->mVertices[i].x;
                vector.y = mesh->mVertices[i].y;
                vector.z = mesh->mVertices[i].z;
                vertex.Position = vector;
                // normals
                if (mesh->HasNormals())
                {
                    vector.x = mesh->mNormals[i].x;
                    vector.y = mesh->mNormals[i].y;
                    vector.z = mesh->mNormals[i].z;
                    vertex.Normal = vector;
                }
                // texture coordinates
                if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
                {
                    glm::vec2 vec;
                    // a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't 
                    // use models where a vertex can have multiple texture coordinates so we always take the first set (0).
                    vec.x = mesh->mTextureCoords[0][i].x;
                    vec.y = mesh->mTextureCoords[0][i].y;
                    vertex.TexCoords = vec;
                    // tangent
                    vector.x = mesh->mTangents[i].x;
                    vector.y = mesh->mTangents[i].y;
                    vector.z = mesh->mTangents[i].z;
                    vertex.Tangent = vector;
                    // bitangent
                    vector.x = mesh->mBitangents[i].x;
                    vector.y = mesh->mBitangents[i].y;
                    vector.z = mesh->mBitangents[i].z;
                    vertex.Bitangent = vector;
                }
                else
                    vertex.TexCoords = glm::vec2(0.0f, 0.0f);

                vertices.push_back(vertex);
            }
            // now wak through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
            for (unsigned int i = 0; i < mesh->mNumFaces; i++)
            {
                aiFace face = mesh->mFaces[i];
                // retrieve all indices of the face and store them in the indices vector
                for (unsigned int j = 0; j < face.mNumIndices; j++)
                    indices.push_back(face.mIndices[j]);
            }
            // process materials
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
            // we assume a convention for sampler names in the shaders. Each diffuse texture should be named
            // as 'texture_diffuseN' where N is a sequential number ranging from 1 to MAX_SAMPLER_NUMBER. 
            // Same applies to other texture as the following list summarizes:
            // diffuse: texture_diffuseN
            // specular: texture_specularN
            // normal: texture_normalN

            // 1. diffuse maps
            std::vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
            textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
            // 2. specular maps
            std::vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
            textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
            // 3. normal maps
            std::vector<Texture> normalMaps = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
            textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
            // 4. height maps
            std::vector<Texture> heightMaps = loadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height");
            textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());

            // return a mesh object created from the extracted mesh data
            return Mesh(vertices, indices, textures);
        }

        // checks all material textures of a given type and loads the textures if they're not loaded yet.
        // the required info is returned as a Texture struct.
        std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName)
        {
            std::vector<Texture> textures;
            for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
            {
                aiString str;
                mat->GetTexture(type, i, &str);
                // check if texture was loaded before and if so, continue to next iteration: skip loading a new texture
                bool skip = false;
                for (unsigned int j = 0; j < textures_loaded.size(); j++)
                {
                    if (std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0)
                    {
                        textures.push_back(textures_loaded[j]);
                        skip = true; // a texture with the same filepath has already been loaded, continue to next one. (optimization)
                        break;
                    }
                }
                if (!skip)
                {   // if texture hasn't been loaded already, load it
                    Texture texture;
                    texture.id = TextureFromFile(str.C_Str(), this->directory);
                    texture.type = typeName;
                    texture.path = str.C_Str();
                    textures.push_back(texture);
                    textures_loaded.push_back(texture);  // store it as texture loaded for entire model, to ensure we won't unnecesery load duplicate textures.
                }
            }
            return textures;
        }
    };


    unsigned int TextureFromFile(const char* path, const std::string& directory, bool gamma)
    {
        std::string filename = std::string(path);
        filename = directory + '/' + filename;

        unsigned int textureID;
        glGenTextures(1, &textureID);

        int width, height, nrComponents;
        unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
        if (data)
        {
            GLenum format;
            if (nrComponents == 1)
                format = GL_RED;
            else if (nrComponents == 3)
                format = GL_RGB;
            else if (nrComponents == 4)
                format = GL_RGBA;

            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            stbi_image_free(data);
        }
        else
        {
            std::cout << "Texture failed to load at path: " << path << std::endl;
            stbi_image_free(data);
        }

        return textureID;
    }

    
}
