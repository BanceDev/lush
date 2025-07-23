pipeline {
    agent any

    stages {
        stage('Build Application Image') {
            steps {
                script {
                    echo 'Initializing Git submodules...'
                    sh 'git submodule update --init --recursive'

                    // Debug: Check if submodules are populated ON JENKINS AGENT
                    echo 'Checking submodule status on Jenkins agent...'
                    sh 'git submodule status'
                    
                    echo 'Checking lib directory contents on Jenkins agent...'
                    sh 'ls -la lib/ || echo "lib directory not found"'
                    sh 'find lib/ -name "*.c" -o -name "*.h" | head -10 || echo "No C files found in lib"'
                    sh 'ls -la lib/compat53/ || echo "compat53 not found"'
                    sh 'ls -la lib/compat53/c-api/ || echo "c-api directory not found"'

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
print("--- Running Lua 5.2 Compatibility Test ---")
print("Basic print test: Hello from Lush!")
print("--- Test Complete ---")
EOF
                        '''
                        
                        echo 'Creating and starting the build container...'
                        sh "docker run -d --name ${containerName} lush-app:latest sleep infinity"

                        echo 'Copying complete workspace (with populated submodules) into container...'
                        sh "docker cp . ${containerName}:/app/"

                        // Debug: Check what's actually in the container AFTER copying
                        echo 'Debugging: Checking container contents AFTER workspace copy...'
                        sh "docker exec ${containerName} ls -la /app/lib/ || echo 'lib directory empty or missing in container'"
                        sh "docker exec ${containerName} ls -la /app/lib/compat53/ || echo 'compat53 not found in container'"
                        sh "docker exec ${containerName} ls -la /app/lib/compat53/c-api/ || echo 'c-api not found in container'"
                        sh "docker exec ${containerName} find /app/lib/ -name '*.c' | head -5 || echo 'No C files found in container lib'"

                        echo 'Copying test script into the container...'
                        sh "docker cp test_52.lua ${containerName}:/app/test_52.lua"

                        echo 'Running premake5 to see what it generates...'
                        sh "docker exec ${containerName} /bin/bash -c 'cd /app && premake5 gmake2'"
                        
                        echo 'Checking generated Makefile...'
                        sh "docker exec ${containerName} /bin/bash -c 'cd /app && head -50 lush.make | grep -i compat || echo \"No compat references found\"'"

                        echo 'Attempting to compile...'
                        sh "docker exec ${containerName} /bin/bash -c 'cd /app && make'"

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