pipeline {
    agent any

    stages {
        stage('Build Application Image') {
            steps {
                script {
                    echo 'Initializing Git submodules...'
                    sh 'git submodule update --init --recursive'

                    echo 'Building the Lush application Docker image...'
                    sh 'docker build -t lush-app:latest .'
                }
            }
        }
        stage('Compile & Test Project') {
            steps {
                script {
                    def containerName = "lush-build-${BUILD_NUMBER}"
                    
                    try {
                        echo 'Creating test script on Jenkins agent...'
                        sh '''
                        cat <<'EOF' > test_52.lua
-- Lua 5.2-specific features test
print("--- Running Lua 5.2 Compatibility Test ---")

-- Test 1: Basic functionality
print("Basic print test: Hello from Lush!")

-- Test 2: Bitwise operations using the preloaded bit32 library
-- The lush host preloads compat modules, so we don't need to 'require' it.
local a, b = 5, 3
print("Bitwise AND of", a, "and", b, "=", bit32.band(a, b))
print("Bitwise OR of", a, "and", b, "=", bit32.bor(a, b))

-- Test 3: load() function (replaces loadstring in Lua 5.2)
local f, err = load("return 10 + 20")
if f then
    print("Loaded function result:", f())
else
    print("Failed to load function:", err)
end

-- Test 4: table.unpack
local t = {1, 2, 3}
print("Unpacked values:", table.unpack(t))

-- Test 5: String operations
local str = "Hello, World!"
print("String length:", #str)
print("Substring:", string.sub(str, 1, 5))

-- Test 6: Math operations
print("Math operations:")
print("  sqrt(16) =", math.sqrt(16))
print("  max(10, 20, 5) =", math.max(10, 20, 5))

print("--- Test Complete: All basic features working ---")
EOF
                        '''
                        
                        echo 'Creating and starting the build container...'
                        sh "docker run -d --name ${containerName} lush-app:latest sleep infinity"

                        echo 'Copying complete workspace (with populated submodules) into container...'
                        // This ensures the populated lib/ directory gets into the container
                        sh "docker cp . ${containerName}:/app/"

                        echo 'Copying test script into the container...'
                        sh "docker cp test_52.lua ${containerName}:/app/test_52.lua"

                        echo 'Compiling and running tests inside the container...'
                        sh "docker exec ${containerName} /bin/bash -c 'cd /app && premake5 gmake2 && make && ./bin/Debug/lush/lush test_52.lua'"

                        echo 'Extracting compiled binary from the container...'
                        sh "docker cp ${containerName}:/app/bin ./"

                    } finally {
                        echo "Cleaning up build container: ${containerName}"
                        sh "docker stop ${containerName} >/dev/null 2>&1 || true"
                        sh "docker rm ${containerName} >/dev/null 2>&1 || true"
                    }
                }
            }
        }
    }
    post {
        always {
            echo 'Pipeline finished.'
            sh 'docker rmi lush-app:latest || true'
        }
    }
}